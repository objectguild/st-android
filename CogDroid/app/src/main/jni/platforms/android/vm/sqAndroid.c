/* sqAndroid.c
 * Initialize the VM here. In order to do this, call interp_init (formerly main) with
 * the zeroth argument pointing to the image plus some fake executable name. This will
 * give the VM an idea where the image is.
 *
 *   Copyright (C) 2010-2017 by
 *   Andreas Raab, Jean Baptiste Arnaud, Santiago Bragagnolo, Dmitry Golubovsky, Michael Rueger
 *   All rights reserved.
 *
 *   Permission is hereby granted, free of charge, to any person obtaining a
 *   copy of this software and associated documentation files (the "Software"),
 *   to deal in the Software without restriction, including without limitation
 *   the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *   and/or sell copies of the Software, and to permit persons to whom the
 *   Software is furnished to do so, subject to the following conditions:
 *
 *   The above copyright notice and this permission notice shall be included in
 *   all copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *   DEALINGS IN THE SOFTWARE.
 */

#include <sys/types.h>
#include <fcntl.h>
#include "sqVirtualMachine.h"
#include <setjmp.h>	

#include <alloca.h>
#include <stdlib.h>
#include <stdio.h>

#include "sqAndroidLogger.c"
#include <jni.h>
#include <sq.h>

#define MAXPATHLEN 256

jmp_buf jmpBufEnter;
extern struct VirtualMachine *interpreterProxy;
int scrw =0, scrh=0;

JNIEnv *CogEnv = NULL;
jobject CogVM = NULL;

jclass vmClass = 0;
jmethodID invalidate;

void setupJNI(JNIEnv *jniEnv, jobject jniVM) {
	CogEnv = jniEnv;
	CogVM = (*jniEnv)->NewGlobalRef(jniEnv, jniVM);
	if (vmClass == 0) {
		jnilog("get class 1");
		jclass cls1 = (*CogEnv)->GetObjectClass(CogEnv, jniVM);
		if (cls1 == 0) {
			jnilog("no class");
		} else {
			vmClass = (*CogEnv)->NewGlobalRef(CogEnv, cls1);
		}
		jnilog("get invalidate");
		invalidate = (*CogEnv)->GetMethodID(CogEnv, vmClass, "invalidate", "(IIII)V");
	}
}

int Java_org_smalltalk_android_display_DisplayView_setScreenSize(JNIEnv *env, jobject self, int w, int h) {
	jnilog("Java_org_smalltalk_android_display_DisplayView_setScreenSize");
	scrw = w;
	scrh = h;
	return 0;
}

int Java_org_smalltalk_android_display_DisplayView_updateDisplay(JNIEnv *env, jobject self,
																 jintArray bits, int w, int h,
																 int d, int left, int top, int right, int bottom) {

	jnilog("Java_org_smalltalk_android_display_DisplayView_updateDisplay");
	int row;
	sqInt formObj = interpreterProxy->displayObject();
	sqInt formBits = interpreterProxy->fetchPointerofObject(0, formObj);
	sqInt width = interpreterProxy->fetchIntegerofObject(1, formObj);
	sqInt height = interpreterProxy->fetchIntegerofObject(2, formObj);
	sqInt depth = interpreterProxy->fetchIntegerofObject(3, formObj);
	int *dispBits = interpreterProxy->firstIndexableField(formBits);
	for(row = top; row < bottom; row++) {
		int ofs = width*row+left;
		(*env)->SetIntArrayRegion(env, bits, ofs, right-left, dispBits+ofs);
	}
	return 1;
}

int Java_org_smalltalk_android_display_DisplayView_sendKeyboardEvent(JNIEnv *env, jobject self, int arg3, int arg4, int arg5, int arg6) {
	jnilog("Java_org_smalltalk_android_display_DisplayView_sendKeyboardEvent");
//	int empty = iebEmptyP();
//	recordKeyboardEvent(arg3, arg4, arg5, arg6);
	return runVM();
}

int Java_org_smalltalk_android_display_DisplayView_sendTouchEvent(JNIEnv *env, jobject self, int arg3, int arg4, int arg5) {
	jnilog("Java_org_smalltalk_android_display_DisplayView_sendTouchEvent");
//	int empty = iebEmptyP();
//	mousePosition.x = arg3;
//	mousePosition.y = arg4;
//	buttonState = arg5;
//	recordMouseEvent();
	return runVM();
}

int Java_org_smalltalk_android_vm_VM_runVM(JNIEnv *env, jobject self) {
	jnilog("Java_org_smalltalk_android_vm_VM_runVM");
//	JNIEnv *oldEnv = CogEnv;
//	jobject *oldCog = CogVM;
	CogEnv = env;
//	CogVM = (*env)->NewLocalRef(env, self);
	int rc = runVM();
//	(*env)->DeleteLocalRef(env, COGVM);
//	CogEnv = oldEnv;
//	CogVM = oldCog;
	return rc;
}
/*
static JavaVM *java_vm;

jint JNI_OnLoad(JavaVM *vm, void *reserved) {
	jnilog("JNI_OnLoad");
	java_vm = vm;

	// Get JNI Env for all function calls
	JNIEnv* jniEnv;
	if ((*vm)->GetEnv(vm, (void **) &jniEnv, JNI_VERSION_1_6) != JNI_OK) {
		jnilog ("GetEnv failed.");
		return -1;
	}

	jnilog ("FindClass");
	jclass 	cls1 = (*jniEnv)->FindClass(jniEnv, "org/smalltalk/android/vm/VM");
	return JNI_VERSION_1_6;
}
*/
int Java_org_smalltalk_android_vm_VM_launchImage(
		JNIEnv *jniEnv,
		jobject jVMObject,
		jstring executablePath_,
		jstring vmOptions_,
		jstring imagePath_,
		jstring command_,
		int w,
		int h
		) {
	start_logger();
	jnilog("Java_org_smalltalk_android_vm_VM_launchImage");
	setupJNI(jniEnv, jVMObject);

	scrw = w;
	scrh = h;

	const char *jexePath = strdup((*jniEnv)->GetStringUTFChars(jniEnv, executablePath_, 0));
	const char *jvmOptions = strdup((*jniEnv)->GetStringUTFChars(jniEnv, vmOptions_, 0));
	const char *jimgpath = strdup((*jniEnv)->GetStringUTFChars(jniEnv, imagePath_, 0));
	const char *jcmd = strdup((*jniEnv)->GetStringUTFChars(jniEnv, command_, 0));
	const char *exePath = strdup(jexePath);
	const char *vmOptions = strdup(jvmOptions);
	const char *imgpath = strdup(jimgpath);
	const char *cmd = strdup(jcmd);

	(*jniEnv)->ReleaseStringUTFChars(jniEnv, executablePath_, jexePath);
	(*jniEnv)->ReleaseStringUTFChars(jniEnv, imagePath_, jvmOptions);
	(*jniEnv)->ReleaseStringUTFChars(jniEnv, imagePath_, jimgpath);
	(*jniEnv)->ReleaseStringUTFChars(jniEnv, command_, jcmd);

	int i,j,z,rc;

	int maxOptions = 128;
	char *optionsv[maxOptions];
	int optionargc = splitcmd(vmOptions, maxOptions, optionsv);

	int maximgarg = 128;
	char *imgargv[maximgarg];
	int imgargc = splitcmd(cmd, maximgarg, imgargv);

	int argl = 2 + + optionargc + imgargc + 1;
	char **argc = alloca(sizeof(char *) * argl);

	argc[0] = exePath;
	i = 1;
	for(j = 0; j < optionargc; j++, i++) argc[i] = optionsv[j];

	for(j = 0; j < imgargc; j++, i++) argc[i] = imgargv[j];

	argc[i++] = imgpath;

	argc[i] = NULL;
	char *envp[] = {NULL};

	rc = main(argl - 1, argc, envp);

//	Java_org_smalltalk_android_vm_VM_runVM(jniEnv, jVMObject);

	return rc;
}

void Java_org_smalltalk_android_vm_VM_surelyExit(JNIEnv *env, jobject self) {
	jnilog("Java_org_smalltalk_android_vm_VM_surelyExit");
	exit(0);
}



void jumpOut(int reasonOfTheJumpOut) {
	jnilog("would jump out " + reasonOfTheJumpOut);
//	longjmp(jmpBufEnter, reasonOfTheJumpOut);
}

int runVM() {
	jnilog("runVM");
//	int reasonOfLeaving = setjmp(jmpBufEnter);
//	jnilog("setjmp " + reasonOfLeaving);
//	if (reasonOfLeaving == 0) {
//		printPhaseTime(2);
		jnilog("interpret");
		interpret();
		jnilog("interpret done");
	return 0;
//	} else {
//		jnilog("leaving");
//	}
//	return reasonOfLeaving;
}

int splitcmd(char *cmd, int maxargc, char **argv) {
	char *argbuf = alloca(strlen(cmd) + 1);
	memset(argbuf, 0, strlen(cmd) + 1);
	int argc = 0;
	int inquote = 0, inarg = 0, inesc = 0;
	int argidx = 0;
	char *cptr;
	for(cptr = cmd; ; cptr++) {
		char c = *cptr;
		if(!c) break;
		if(argc >= maxargc) return argc;
		if(inesc) {
			argbuf[argidx++] = c;
			inesc = 0;
			continue;
		}
		if(c == '\\' && !inesc) {
			inesc = 1;
			continue;
		}
		if(c == '\"') {
			inquote = ~inquote;
			continue;
		}
				if(inquote) {
			argbuf[argidx++] = c;
			continue;
		}
		if((c == ' ' || c == '\t') && !inquote) {
			if(!strlen(argbuf)) continue;
			else {
				argv[argc] = strdup(argbuf);
				argc++;
				memset(argbuf, 0, strlen(cmd) + 1);
				argidx = 0;
			}
			continue;
		}
		argbuf[argidx++] = c;
	}
	if(strlen(argbuf)) {
		argv[argc] = strdup(argbuf);
		argc++;
	}
	return argc;
}