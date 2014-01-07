/*
	scrypt_common.cc

	Copyright (C) 2013 Barry Steyn (http://doctrina.org/Scrypt-Authentication-For-Node.html)

	This source code is provided 'as-is', without any express or implied
	warranty. In no event will the author be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	1. The origin of this source code must not be misrepresented; you must not
	claim that you wrote the original source code. If you use this source code
	in a product, an acknowledgment in the product documentation would be
	appreciated but is not required.

	2. Altered source versions must be plainly marked as such, and must not be
	misrepresented as being the original source code.

	3. This notice may not be removed or altered from any source distribution.

	Barry Steyn barry.steyn@gmail.com

*/


//
// Common functionality needed by boiler plate code
//

#include <v8.h>
#include <node.h>
#include <node_buffer.h>
#include <string>
#include "common.h"

namespace Internal {
	namespace {
	//
	// Returns error descriptions as generated by Scrypt
	//
	std::string ScryptErrorDescr(const int error) {
		switch(error) {
			case 0: 
				return "success";
			case 1: 
				return "getrlimit or sysctl(hw.usermem) failed";
			case 2: 
				return "clock_getres or clock_gettime failed";
			case 3: 
				return "error computing derived key";
			case 4: 
				return "could not read salt from /dev/urandom";
			case 5: 
				return "error in OpenSSL";
			case 6: 
				return "malloc failed";
			case 7: 
				return "data is not a valid scrypt-encrypted block";
			case 8: 
				return "unrecognized scrypt format";
			case 9:     
				return "decrypting file would take too much memory";
			case 10: 
				return "decrypting file would take too long";
			case 11: 
				return "password is incorrect";
			case 12: 
				return "error writing output file";
			case 13: 
				return "error reading input file";
			default:
				return "error unkown";
		}
	}

	//
	// Used for Buffer constructor, needed so that data can be used and not deep copied
	// for an excellent example by Ben Noordhuis, see https://groups.google.com/forum/#!topic/nodejs/gz8YF3oLit0
	//
	void FreeCallback(char* data, void* hint) {
		delete [] data;
	} 

	} //end anon namespace

	using namespace v8;
	
	//
	// Checks that ScryptParams object is "Kosher"
	//
	int CheckScryptParameters(const Local<Object> &obj, std::string& errMessage) {
		Local<Value> val;
		if (!obj->Has(String::New("N"))) {
			errMessage = "N value is not present";
			return PARMOBJ;
		}

		if (!obj->Has(String::New("r"))) {
			errMessage = "r value is not present";
			return PARMOBJ;
		}

		if (!obj->Has(String::New("p"))) {
			errMessage = "p value is not present";
			return PARMOBJ;
		}

		val = obj->Get(String::New("N"));
		if (!val->IsNumber()) {
			errMessage = "N must be a numeric value";
			return PARMOBJ;
		}
		
		val = obj->Get(String::New("r"));
		if (!val->IsNumber()) {
			errMessage = "r must be a numeric value";
			return PARMOBJ;
		}
		
		val = obj->Get(String::New("p"));
		if (!val->IsNumber()) {
			errMessage = "p must be a numeric value";
			return PARMOBJ;
		}
		
		return 0;
	}

	//
	// Definition for assignment operator
	//
	void ScryptParams::operator=(const Local<Object> &rhs) {
		this->N = rhs->Get(String::New("N"))->ToNumber()->Value();
		this->r = rhs->Get(String::New("r"))->ToNumber()->Value();
		this->p = rhs->Get(String::New("p"))->ToNumber()->Value();
	}

	//
	// Produces a JSON error object
	//
	Local<Value> MakeErrorObject(int errorCode, std::string& errorMessage) {

		if (errorCode) {
			Local<Object> errorObject = Object::New();
			
			switch (errorCode) {
				case ADDONARG:
						errorMessage = "Module addon argument error: "+errorMessage;
					break;

				case JSARG:
						errorMessage = "JavaScript wrapper argument error: "+errorMessage;
					break;

				case PARMOBJ:
						errorMessage = "Scrypt parameter object error: "+errorMessage;
					break;

				case CONFIG:
						errorMessage = "Scrypt config object error: "+errorMessage;
					break;

				default:
					errorCode = 500;
					errorMessage = "Unknown internal error - please report this error to make this module better. Details about error reporting can be found at the GitHub repo: https://github.com/barrysteyn/node-scrypt#report-errors";
			}
			errorObject->Set(String::NewSymbol("err_code"), Integer::New(errorCode));
			errorObject->Set(String::NewSymbol("err_message"), String::New(errorMessage.c_str()));

			return errorObject;
		}
	
		return Local<Value>::New(Null());
	}

	//
	// Produces a JSON error object for errors resulting from Scrypt
	//
	Local<Value> MakeErrorObject(int errorCode, int scryptErrorCode) {
		assert(errorCode == SCRYPT);
		
		if (scryptErrorCode) { 
			Local<Object> errorObject = Object::New();
			errorObject->Set(String::NewSymbol("err_code"), Integer::New(errorCode));
			errorObject->Set(String::NewSymbol("err_message"), String::New("Scrypt error"));
			errorObject->Set(String::NewSymbol("scrypt_err_code"),Integer::New(scryptErrorCode));
			errorObject->Set(String::NewSymbol("scrypt_err_message"),String::New(ScryptErrorDescr(scryptErrorCode).c_str()));
			return errorObject;
		}

		return Local<Value>::New(Null());
	}

	//
	// Create a "fast" NodeJS Buffer
	//
	void
	CreateBuffer(Handle<Value> &buffer, size_t dataLength) {
		node::Buffer *slowBuffer = node::Buffer::New(dataLength);

		//Create the node JS "fast" buffer
		Local<Object> globalObj = Context::GetCurrent()->Global();  
		Local<Function> bufferConstructor = Local<Function>::Cast(globalObj->Get(String::New("Buffer")));
		Handle<Value> constructorArgs[3] = { slowBuffer->handle_, Integer::New(dataLength), Integer::New(0) };

		//Create the "fast buffer"
		buffer = bufferConstructor->NewInstance(3, constructorArgs);
	}
	
	//
	// Create a "fast" NodeJS Buffer from an already declared memory heap
	//
	void
	CreateBuffer(Handle<Value> &buffer, char* data, size_t dataLength) {
		node::Buffer *slowBuffer = node::Buffer::New(data, dataLength, FreeCallback, NULL);

		//Create the node JS "fast" buffer
		Local<Object> globalObj = Context::GetCurrent()->Global();  
		Local<Function> bufferConstructor = Local<Function>::Cast(globalObj->Get(String::New("Buffer")));
		Handle<Value> constructorArgs[3] = { slowBuffer->handle_, Integer::New(dataLength), Integer::New(0) };

		//Create the "fast buffer"
		buffer = bufferConstructor->NewInstance(3, constructorArgs);
	}

	//
	// Produces a nodejs Buffer argument
	//
	int
	ProduceBuffer(Handle<Value>& argument, const std::string& argName, std::string& errorMessage, const node::encoding& encoding, bool checkEmpty) {
		size_t dataLength = 0;
		char *data = NULL;

		if (encoding == node::BUFFER && node::Buffer::HasInstance(argument->ToObject())) {
			return 0;
		}
	
		if (!argument->IsString() && !argument->IsStringObject() && !node::Buffer::HasInstance(argument->ToObject())) {
			errorMessage = argName + " must be a buffer or string";
			return 1;
		}

		if (encoding == node::BUFFER && !node::Buffer::HasInstance(argument->ToObject())) {
			errorMessage = argName + " must be a buffer as specified by config";
			return 1;
		}

		//Create a buffer with a string
		if (argument->IsString() || argument->IsStringObject()) {
			Handle<Value> buffer;
			size_t dataWritten = 0;

			dataLength = node::DecodeBytes(argument, encoding);
			data = new char[dataLength];
			CreateBuffer(buffer, data, dataLength);
			dataWritten = node::DecodeWrite(data, dataLength, argument, encoding);
			assert(dataWritten == dataLength);

			if (dataWritten != dataLength) {
				errorMessage = argName + " is probably encoded differently to what was specified";
				return 1;
			}

			argument = buffer;
		}

		if (checkEmpty && node::Buffer::Length(argument) == 0) {
			errorMessage = argName + " cannot be empty";
			return 1;
		}

		return 0;
	}
} //end Internal namespace
