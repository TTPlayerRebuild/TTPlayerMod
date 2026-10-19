#include "io.h"
#include <libopenmpt/libopenmpt.h>
#include <cmath>
namespace ttp::mod {
// Mode 2 is original ModPlug's 0.5-bit dither with a fixed PRNG seed.
// The library default is randomly seeded 1-bit noise-shaped dither, which
// changes PCM between opens (even at an identical caller buffer capacity).
class ModuleReader final:public Reader,public Metadata {
 LONG refs_{1};std::unique_ptr<Input> input_;openmpt_module* module_{};
 WAVEFORMATEX format_{WAVE_FORMAT_PCM,2,44100,176400,4,16,0};DWORD duration_{};std::wstring title_,codec_;
 static void log(const char*,void*){}static int error(int,void*){return OPENMPT_ERROR_FUNC_RESULT_STORE;}
 template<class T>HRESULT property(T*p,T value){if(!p)return E_POINTER;*p={};if(!module_)return E_UNEXPECTED;*p=value;return S_OK;}
 std::wstring metadata(const char*key){auto s=openmpt_module_get_metadata(module_,key);std::wstring out;if(s){out=wide(s);openmpt_free_string(s);}return out;}
 void status(){require(openmpt_module_error_get_last(module_)==OPENMPT_ERROR_OK,HRESULT_FROM_WIN32(ERROR_INVALID_DATA));}
public:
 ~ModuleReader(){if(module_)openmpt_module_destroy(module_);}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void**p)override{if(!p)return E_POINTER;*p=nullptr;if(same(iid,IID_IUnknown)||same(iid,iid_reader))*p=static_cast<Reader*>(this);else if(same(iid,iid_metadata))*p=static_cast<Metadata*>(this);else return E_NOINTERFACE;AddRef();return S_OK;}
 ULONG STDMETHODCALLTYPE AddRef()override{return InterlockedIncrement(&refs_);}ULONG STDMETHODCALLTYPE Release()override{auto n=InterlockedDecrement(&refs_);if(!n)delete this;return n;}
 HRESULT STDMETHODCALLTYPE Open(IStream*s,DWORD)override{if(module_)return E_UNEXPECTED;auto hr=protect([&]{input_=std::make_unique<Input>();input_->open(s);constexpr uint64_t maximum=256ull*1024*1024;require(input_->size<=maximum,HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE));Bytes bytes;bytes.reserve(size_t(input_->size));BYTE chunk[65536];for(;;){require(WaitForSingleObject(input_->event,0)!=WAIT_OBJECT_0,E_ABORT);auto got=input_->read(chunk,sizeof(chunk));if(!got)break;require(bytes.size()+got<=maximum,HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE));bytes.insert(bytes.end(),chunk,chunk+got);}require(!bytes.empty());int err{};const char*message{};const openmpt_module_initial_ctl settings[]={{"load.skip_plugins","1"},{"seek.sync_samples","1"},{"dither","2"},{nullptr,nullptr}};module_=openmpt_module_create_from_memory2(bytes.data(),bytes.size(),log,nullptr,error,nullptr,&err,&message,settings);openmpt_free_string(message);require(module_!=nullptr);require(openmpt_module_set_repeat_count(module_,0)!=0);require(openmpt_module_set_render_param(module_,OPENMPT_MODULE_RENDER_INTERPOLATIONFILTER_LENGTH,8)!=0);auto seconds=openmpt_module_get_duration_seconds(module_);require(std::isfinite(seconds)&&seconds>0);duration_=dword(uint64_t(seconds*1000));title_=metadata("title");codec_=metadata("type");auto name=metadata("type_long");codec_+=L"|"+name;status();return S_OK;});if(FAILED(hr)){if(module_)openmpt_module_destroy(module_);module_=nullptr;input_.reset();}return hr;}
 HRESULT STDMETHODCALLTYPE Capabilities(DWORD*p)override{return property(p,DWORD(2|(input_&&input_->remote?1:0)));}
 HRESULT STDMETHODCALLTYPE Duration(DWORD*p)override{return property(p,duration_);}
 HRESULT STDMETHODCALLTYPE Format(WAVEFORMATEX**p)override{if(!p)return E_POINTER;*p=nullptr;if(!module_)return E_UNEXPECTED;*p=static_cast<WAVEFORMATEX*>(CoTaskMemAlloc(sizeof(format_)));if(!*p)return E_OUTOFMEMORY;**p=format_;return S_OK;}
 HRESULT STDMETHODCALLTYPE BufferSize(DWORD*p)override{return property(p,DWORD(4096));}
 HRESULT STDMETHODCALLTYPE CodecName(wchar_t**p)override{return text(codec_,p);}
 HRESULT STDMETHODCALLTYPE Bitrate(DWORD*p)override{return property(p,duration_&&input_?dword(input_->size*8000/duration_):0u);}
 HRESULT STDMETHODCALLTYPE BitDepth(WORD*p)override{return property(p,WORD(16));}
 HRESULT STDMETHODCALLTYPE SetCallback(IUnknown*p)override{return input_?input_->callback(p):E_UNEXPECTED;}
 HRESULT STDMETHODCALLTYPE Start()override{if(!module_)return E_UNEXPECTED;ResetEvent(input_->event);return S_OK;}
 HRESULT STDMETHODCALLTYPE Stop()override{if(!module_)return E_UNEXPECTED;SetEvent(input_->event);return S_OK;}
 HRESULT STDMETHODCALLTYPE Read(Buffer*b)override{return protect([&]()->HRESULT{require(b,E_POINTER);check(b->SetLength(0));require(module_,E_UNEXPECTED);require(WaitForSingleObject(input_->event,0)!=WAIT_OBJECT_0,E_ABORT);BYTE*p{};DWORD ignored{},cap{};check(b->Data(&p,&ignored));check(b->Capacity(&cap));require(p,E_POINTER);require(cap>=4,E_INVALIDARG);openmpt_module_error_clear(module_);auto frames=openmpt_module_read_interleaved_stereo(module_,44100,cap/4,reinterpret_cast<int16_t*>(p));status();require(frames<=cap/4);check(b->SetLength(DWORD(frames*4)));return frames?S_OK:S_FALSE;});}
 HRESULT STDMETHODCALLTYPE Seek(DWORD*p)override{return protect([&]{require(p,E_POINTER);require(module_,E_UNEXPECTED);openmpt_module_error_clear(module_);auto seconds=openmpt_module_set_position_seconds(module_,std::min(*p,duration_)/1000.);status();require(std::isfinite(seconds)&&seconds>=0);*p=dword(uint64_t(seconds*1000));ResetEvent(input_->event);return S_OK;});}
 HRESULT STDMETHODCALLTYPE Count(DWORD*p)override{return property(p,DWORD(1));}
 HRESULT STDMETHODCALLTYPE At(DWORD i,wchar_t**k,wchar_t**v)override{if(!k||!v)return E_POINTER;*k=*v=nullptr;if(i)return E_INVALIDARG;auto hr=text(L"Title",k);if(FAILED(hr))return hr;hr=text(title_,v);if(FAILED(hr)){CoTaskMemFree(*k);*k=nullptr;}return hr;}
 HRESULT STDMETHODCALLTYPE Get(const char*k,wchar_t**v)override{if(!k||!v)return E_POINTER;*v=nullptr;return !_stricmp(k,"Title")?text(title_,v):E_INVALIDARG;}
 HRESULT STDMETHODCALLTYPE Set(const char*,const wchar_t*)override{return E_NOTIMPL;}
};
HRESULT make_reader(void**p){if(!p)return E_POINTER;*p=nullptr;return protect([&]{*p=static_cast<Reader*>(new ModuleReader);return S_OK;});}
}
