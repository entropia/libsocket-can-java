SHELL=/bin/bash
CXX=g++
NAME:=libsocket-can-java

### JAVA_HOME
ifndef JAVA_HOME
	JAVA_HOME=$(shell readlink -f /usr/bin/javac | sed "s:bin/javac::")
endif

JAVA_INCLUDES=-I$(JAVA_HOME)/include
JAVA=$(JAVA_HOME)/bin/java
JAVAC=$(JAVA_HOME)/bin/javac
JAVAH=$(JAVA_HOME)/bin/javah
JAR=$(JAVA_HOME)/bin/jar
JAVA_SRC:=$(shell find src -type f -and -name '*.java')
JAVA_TEST_SRC:=$(shell find src.test -type f -and -name '*.java')
JNI_SRC:=$(shell find jni -type f -and -regex '^.*\.\(cpp\|h\)$$')
JAVA_DEST=classes
JAVA_TEST_DEST=classes.test
LIB_DEST=lib
JAR_DEST=dist
JAR_DEST_FILE=$(JAR_DEST)/$(NAME).jar
DIRS=stamps obj $(JAVA_DEST) $(JAVA_TEST_DEST) $(LIB_DEST) $(JAR_DEST)
JNI_DIR=jni
JNI_CLASSES=de.entropia.can.CanSocket
JAVAC_FLAGS=-g -Xlint:all
CXXFLAGS=-I./include -O2 -g -pipe -Wall -Wp,-D_FORTIFY_SOURCE=2 -fexceptions \
-fstack-protector --param=ssp-buffer-size=4 -fPIC \
-pedantic -std=gnu++11 -D_REENTRANT -D_GNU_SOURCE \
$(JAVA_INCLUDES)
SONAME=jni_socketcan
LDFLAGS=-Wl,-soname,$(SONAME)

.DEFAULT_GOAL := all
.LIBPATTERNS :=
.SUFFIXES:

.PHONY: all
all: stamps/create-jar stamps/compile-test

.PHONY: clean
clean:
	$(RM) -r $(DIRS) $(STAMPS) $(filter %.h,$(JNI_SRC))

stamps/dirs:
	mkdir $(DIRS)
	@touch $@

stamps/compile-src: stamps/dirs $(JAVA_SRC)
	$(JAVAC) $(JAVAC_FLAGS) -d $(JAVA_DEST) $(sort $(JAVA_SRC))
	@touch $@

stamps/compile-test: stamps/compile-src $(JAVA_TEST_SRC)
	$(JAVAC) $(JAVAC_FLAGS) -cp $(JAVA_DEST) -d $(JAVA_TEST_DEST) \
		$(sort $(JAVA_TEST_SRC))
	@touch $@

stamps/generate-jni-h: stamps/compile-src
	$(JAVAH) -jni -d $(JNI_DIR) -classpath $(JAVA_DEST) \
		$(JNI_CLASSES)
	@touch $@

stamps/compile-jni: stamps/generate-jni-h $(JNI_SRC)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -shared -o $(LIB_DEST)/lib$(SONAME).so \
		$(sort $(filter %.cpp,$(JNI_SRC)))
	@touch $@

stamps/create-jar: stamps/compile-jni
	$(JAR) cMf $(JAR_DEST_FILE) lib -C $(JAVA_DEST) .
	@touch $@

.PHONY: check
check: stamps/create-jar stamps/compile-test
	$(JAVA) -ea -cp $(JAR_DEST_FILE):$(JAVA_TEST_DEST) \
		-Xcheck:jni \
		de.entropia.can.CanSocketTest
