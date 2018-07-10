#include "PCRE2Wrapper.h"

Nan::Persistent<v8::Function> PCRE2Wrapper::constructor;

PCRE2Wrapper::PCRE2Wrapper() {
	lastIndex = 0;
	global = false;
}

PCRE2Wrapper::~PCRE2Wrapper() {
}

void PCRE2Wrapper::Init(std::string name, v8::Local<v8::Object> exports) {
	Nan::HandleScope scope;

	v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
	tpl->SetClassName(Nan::New(name).ToLocalChecked());
	tpl->InstanceTemplate()->SetInternalFieldCount(1);
	
	Nan::SetPrototypeMethod(tpl, "test", PCRE2Wrapper::Test);
	Nan::SetPrototypeMethod(tpl, "exec", PCRE2Wrapper::Exec);
	Nan::SetPrototypeMethod(tpl, "match", PCRE2Wrapper::Match);
	Nan::SetPrototypeMethod(tpl, "replace", PCRE2Wrapper::Replace);
	
	Nan::SetPrototypeMethod(tpl, "toString", PCRE2Wrapper::ToString);
	
	v8::Local<v8::ObjectTemplate> proto = tpl->PrototypeTemplate();
	Nan::SetAccessor(proto, Nan::New("source").ToLocalChecked(), PCRE2Wrapper::PropertyGetter);
	Nan::SetAccessor(proto, Nan::New("flags").ToLocalChecked(), PCRE2Wrapper::PropertyGetter);
	Nan::SetAccessor(proto, Nan::New("lastIndex").ToLocalChecked(), PCRE2Wrapper::PropertyGetter, PCRE2Wrapper::PropertySetter);
	Nan::SetAccessor(proto, Nan::New("global").ToLocalChecked(), PCRE2Wrapper::PropertyGetter);
	Nan::SetAccessor(proto, Nan::New("ignoreCase").ToLocalChecked(), PCRE2Wrapper::PropertyGetter);
	Nan::SetAccessor(proto, Nan::New("multiline").ToLocalChecked(), PCRE2Wrapper::PropertyGetter);
	Nan::SetAccessor(proto, Nan::New("sticky").ToLocalChecked(), PCRE2Wrapper::PropertyGetter);

	constructor.Reset(tpl->GetFunction());
	exports->Set(Nan::New(name).ToLocalChecked(), tpl->GetFunction());
}

void PCRE2Wrapper::New(const Nan::FunctionCallbackInfo<v8::Value>& info) {
	if ( !info.IsConstructCall() ) {
		Nan::ThrowError("Use `new` to create instances of this object.");
		return;
	}
	
	PCRE2Wrapper* obj = new PCRE2Wrapper();
	obj->Wrap(info.This());
	
	std::string pattern;
	std::string flags;
	
	if ( info[0]->IsString() ) {
		pattern = *Nan::Utf8String(info[0]->ToString());
		flags = info[1]->IsUndefined() ? "" : *Nan::Utf8String(info[1]->ToString());
	}
	else if ( info[0]->IsRegExp() ) {
		v8::Local<v8::RegExp> v8Regexp = info[0].As<v8::RegExp>();
		v8::Local<v8::String> v8Source = v8Regexp->GetSource();
		v8::RegExp::Flags v8Flags = v8Regexp->GetFlags();
		
		pattern = *Nan::Utf8String(v8Source);
		
		if ( bool(v8Flags & v8::RegExp::kIgnoreCase) ) flags += "i";
		if ( bool(v8Flags & v8::RegExp::kMultiline) ) flags += "m";
		if ( bool(v8Flags & v8::RegExp::kGlobal) ) flags += "g";
	}

	if( strchr(flags.c_str(), 'g') ) obj->global = true;
	
	std::string constructorName = *Nan::Utf8String(info.This()->GetConstructorName());
	
	if( constructorName == "PCRE2JIT" ) {
		flags += "S";
	}
	
	obj->re.setPattern(pattern.c_str());
	obj->re.addModifier(flags.c_str());
	obj->re.compile();
	
	obj->flags = flags;
	
	if ( !obj->re ) {	
		Nan::ThrowError(obj->re.getErrorMessage().c_str());
		return;
	}

	info.GetReturnValue().Set(info.This());
}

void PCRE2Wrapper::Test(const Nan::FunctionCallbackInfo<v8::Value>& info) {
	PCRE2Wrapper *obj = ObjectWrap::Unwrap<PCRE2Wrapper>(info.This());
	
	std::string subject = *Nan::Utf8String(info[0]);
	
	jp::RegexMatch& rm = obj->rm;
	
	rm.clear();
	rm.setRegexObject(&obj->re);
	rm.setSubject(subject);
	rm.setModifier(obj->flags);

	rm.setStartOffset(obj->global ? obj->lastIndex : 0);
	rm.setMatchEndOffsetVector(&obj->vec_eoff);
	
	rm.setNumberedSubstringVector(NULL);
	rm.setNamedSubstringVector(NULL);
	rm.setNameToNumberMapVector(NULL);
	
	bool result = (rm.match() > 0);
	
	if ( result && obj->global ) obj->lastIndex = obj->vec_eoff[0];
	
	info.GetReturnValue().Set(result ? Nan::True() : Nan::False());
}

void PCRE2Wrapper::Exec(const Nan::FunctionCallbackInfo<v8::Value>& info) {
	PCRE2Wrapper *obj = ObjectWrap::Unwrap<PCRE2Wrapper>(info.This());
	
	std::string subject = *Nan::Utf8String(info[0]);
	jp::RegexMatch& rm = obj->rm;
	
	rm.clear();
	rm.setRegexObject(&obj->re);
	rm.setSubject(subject);
	rm.setModifier(obj->flags);
	
	rm.setStartOffset(obj->global ? obj->lastIndex : 0);
	rm.setMatchEndOffsetVector(&obj->vec_eoff);
	
	rm.setNumberedSubstringVector(&obj->vec_num);
	rm.setNamedSubstringVector(&obj->vec_nas);
	rm.setNameToNumberMapVector(&obj->vec_ntn);
	
	size_t result_count = rm.match();
	
	if ( result_count == 0 ) {
		obj->lastIndex = 0;
	
		info.GetReturnValue().Set(Nan::Null());
	}
	else {
		size_t finish_offset = obj->vec_eoff[0];
	
		if ( obj->global ) obj->lastIndex = finish_offset;
	
		v8::Local<v8::Array> result = Nan::New<v8::Array>(result_count);
		
		std::string whole_match = obj->vec_num[0][0];
		
		for ( size_t i=0; i<obj->vec_num[0].size(); i++ ) {
			result->Set(i, Nan::New(obj->vec_num[0][i]).ToLocalChecked());
		}
				
		v8::Local<v8::Object> named = Nan::New<v8::Object>();
		result->Set(Nan::New("groups").ToLocalChecked(), named);
		result->Set(Nan::New("named").ToLocalChecked(), named);
	
		for ( auto const& ent : obj->vec_nas[0] ) {
			named->Set(Nan::New(ent.first).ToLocalChecked(), Nan::New(ent.second).ToLocalChecked());
		}
		
		int match_offset = finish_offset - whole_match.length();
		
		result->Set(Nan::New("index").ToLocalChecked(), Nan::New((int32_t)match_offset));
		result->Set(Nan::New("input").ToLocalChecked(), Nan::New(subject).ToLocalChecked());
		
		info.GetReturnValue().Set(result);
	}
}

void PCRE2Wrapper::Match(const Nan::FunctionCallbackInfo<v8::Value>& info) {
	PCRE2Wrapper *obj = ObjectWrap::Unwrap<PCRE2Wrapper>(info.This());
	
	if ( !obj->global ) {
		PCRE2Wrapper::Exec(info);
		return;
	}
	
	std::string subject = *Nan::Utf8String(info[0]);
	jp::RegexMatch& rm = obj->rm;
	
	rm.clear();
	rm.setRegexObject(&obj->re);
	rm.setSubject(subject);
	rm.setModifier("g");
	
	rm.setStartOffset(0);
	rm.setMatchEndOffsetVector(&obj->vec_eoff);

	rm.setNumberedSubstringVector(&obj->vec_num);
	rm.setNamedSubstringVector(NULL);
	rm.setNameToNumberMapVector(NULL);

	size_t result_count = rm.match();
	
	if ( result_count == 0 ) {
		obj->lastIndex = 0;
	
		info.GetReturnValue().Set(Nan::Null());
	}
	else {
		v8::Local<v8::Array> result = Nan::New<v8::Array>(result_count);
		
		for ( size_t i=0; i<obj->vec_num.size(); ++i ) {
			result->Set(i, Nan::New(obj->vec_num[i][0]).ToLocalChecked());
		}
		
		info.GetReturnValue().Set(result);
	}
}

void PCRE2Wrapper::Replace(const Nan::FunctionCallbackInfo<v8::Value>& info) {
	PCRE2Wrapper *obj = ObjectWrap::Unwrap<PCRE2Wrapper>(info.This());
	
	std::string subject = *Nan::Utf8String(info[0]);

	bool withCallback = info[1]->IsFunction();
	
	if ( !withCallback ) {
		const char* replacement = *Nan::Utf8String(info[1]);
		
		jp::RegexReplace & rr = obj->rr;
	
		rr.clear();
		rr.setRegexObject(&obj->re);
		rr.setSubject(subject);
		rr.setModifier(obj->flags);
		
		rr.setStartOffset(0);

		rr.setReplaceWith(replacement);
		
		const char* result = rr.replace().c_str();
		
		info.GetReturnValue().Set(Nan::New(result).ToLocalChecked());
	}
	else {
		v8::Local<v8::Function> callback = info[1].As<v8::Function>();
		
		jp::MatchEvaluator & me = obj->me;
	
		me.clear();
		me.setRegexObject(&obj->re);
		me.setSubject(subject);
		me.setModifier(obj->flags);

		me.setStartOffset(0);
		
		me.setCallback(
			[&me, &obj, &subject, &callback](const jp::NumSub& cg, const jp::MapNas& ng, void*) mutable {
				jp::VecNum const* vec_num = me.getNumberedSubstringVector();
				ptrdiff_t pos = distance(vec_num->begin(), find(vec_num->begin(), vec_num->end(), cg));
				
				const jpcre2::VecOff* vec_soff = me.getMatchStartOffsetVector();
				size_t match_offset = (*vec_soff)[pos];
				
				const unsigned argCount = 3 + cg.size();
				v8::Local<v8::Value> *argVector = new v8::Local<v8::Value>[argCount];

				for ( size_t i=0; i<cg.size(); i++ ) {
					argVector[i] = Nan::New(cg[i]).ToLocalChecked(); //match, p1, p2, ... , pn
				}

				argVector[argCount-3] = Nan::New((uint32_t)match_offset); //offset
				argVector[argCount-2] = Nan::New(subject).ToLocalChecked(); //subject
				
				v8::Local<v8::Object> named = Nan::New<v8::Object>();
				argVector[argCount-1] = named; //named
				
				for ( auto const& ent : ng ) {
					named->Set(Nan::New(ent.first).ToLocalChecked(), Nan::New(ent.second).ToLocalChecked());
				}

				v8::Local<v8::Value> returned = callback->Call(Nan::GetCurrentContext()->Global(), argCount, argVector);

				delete[] argVector;

				return *Nan::Utf8String(returned);
			}
		);
		
		const char* result = me.replace().c_str();
		
		info.GetReturnValue().Set(Nan::New(result).ToLocalChecked());
	}
}

void PCRE2Wrapper::ToString(const Nan::FunctionCallbackInfo<v8::Value>& info) {
	PCRE2Wrapper *obj = ObjectWrap::Unwrap<PCRE2Wrapper>(info.This());
	
	std::string result = "/" + obj->re.getPattern() + "/" + obj->flags;
	
	info.GetReturnValue().Set(Nan::New(result).ToLocalChecked());
}

void PCRE2Wrapper::HasModifier(const Nan::FunctionCallbackInfo<v8::Value>& info) {
	PCRE2Wrapper *obj = ObjectWrap::Unwrap<PCRE2Wrapper>(info.This());
	
	std::string wanted = *Nan::Utf8String(info[0]);
	
	bool result = true;
	
	for(char& c : wanted) {
		if ( obj->flags.find(c) == std::string::npos ) {
			result = false;
			break;
		}
	}
	
	info.GetReturnValue().Set(result ? Nan::True() : Nan::False());
}

void PCRE2Wrapper::PropertyGetter(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value>& info) {
	PCRE2Wrapper *obj = ObjectWrap::Unwrap<PCRE2Wrapper>(info.This());
	
	std::string name = *Nan::Utf8String(property);
	
	if ( name == "source" ) {
		info.GetReturnValue().Set(Nan::New(obj->re.getPattern()).ToLocalChecked());
	}
	else if ( name == "flags" ) {
		std::string modifier = obj->re.getModifier();
		if ( obj->global ) modifier += "g";
		
		info.GetReturnValue().Set(Nan::New(modifier).ToLocalChecked());
	}
	else if ( name == "lastIndex" ) {
		info.GetReturnValue().Set(Nan::New<v8::Integer>((uint32_t)obj->lastIndex));
	}
	else {
		bool result = false;
		
		if ( name == "global" ) result = (obj->flags.find('g') != std::string::npos);
		else if ( name == "sticky" ) result = false;
		else if ( name == "ignoreCase" ) result = (obj->flags.find('i') != std::string::npos);
		else if ( name == "multiline" ) result = (obj->flags.find('m') != std::string::npos);
		
		info.GetReturnValue().Set(Nan::New(result));
	}
}

NAN_SETTER(PCRE2Wrapper::PropertySetter) {
	PCRE2Wrapper *obj = ObjectWrap::Unwrap<PCRE2Wrapper>(info.This());
	
	std::string name = *Nan::Utf8String(property);
	
	if ( name == "lastIndex" ) {
		int32_t val = value->Int32Value();
		obj->lastIndex = val < 0 ? 0 : val;
		info.GetReturnValue().Set(Nan::New<v8::Integer>((uint32_t)obj->lastIndex));
	}
}