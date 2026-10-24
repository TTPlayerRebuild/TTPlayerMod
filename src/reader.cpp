#include "ttpcomm/audio_format_probe_impl.h"
#include "io.h"
#pragma warning(push)
#pragma warning(disable:4005 4100)
#include <libmodplug/stdafx.h>
#pragma push_macro("lstrcpyn")
#undef lstrcpyn
#define lstrcpyn lstrcpynA
#include <libmodplug/sndfile.h>
#pragma pop_macro("lstrcpyn")
#pragma warning(pop)
#include <array>
#include "legacy_mixer.h"

// The original-family engine has global scratch buffers and mixer settings.
// All engine calls are serialized; per-reader DC/ramp history is restored
// around reads/seeks so two active readers do not contaminate each other.
extern LONG gnDryROfsVol, gnDryLOfsVol, gnRvbROfsVol, gnRvbLOfsVol;
namespace ttp::mod {
namespace {
CRITICAL_SECTION engine_lock;
volatile LONG engine_initialized{};
void initialize_engine() {
    if (InterlockedCompareExchange(&engine_initialized, 1, 0) == 0) {
        if (!InitializeCriticalSectionAndSpinCount(&engine_lock, 4000)) {
            InterlockedExchange(&engine_initialized, -1);
            throw Failure{E_OUTOFMEMORY};
        }
        ttp_mod_legacy::InitializeTables();
        CSoundFile::SetWaveConfig(44100, 16, 2, FALSE);
        CSoundFile::SetResamplingMode(3);
        CSoundFile::SetMixConfig(128, 32);
        InterlockedExchange(&engine_initialized, 2);
    } else {
        LONG state;
        while ((state = InterlockedCompareExchange(&engine_initialized, 2, 2)) == 1)
            Sleep(0);
        require(state == 2, E_OUTOFMEMORY);
    }
}
struct EngineGuard {
    EngineGuard() { initialize_engine(); EnterCriticalSection(&engine_lock); }
    ~EngineGuard() { LeaveCriticalSection(&engine_lock); }
};
std::wstring ansi_title(const char *title) {
    if (!title || !*title) return {};
    int count = MultiByteToWideChar(CP_ACP, 0, title, -1, nullptr, 0);
    require(count > 0);
    std::wstring result(count, L'\0');
    require(MultiByteToWideChar(CP_ACP, 0, title, -1, result.data(), count) == count);
    result.pop_back();
    return result;
}
}
class ModuleReader final : public Reader, public Metadata, public ttp::format::InfoSupport {
    LONG refs_{1};
    std::unique_ptr<Input> input_;
    std::unique_ptr<CSoundFile> module_;
    WAVEFORMATEX format_{WAVE_FORMAT_PCM, 2, 44100, 176400, 4, 16, 0};
    DWORD duration_{}, maximum_position_{};
    std::wstring title_;
    std::array<BYTE, 4096> pending_{};
    DWORD pending_size_{}, pending_offset_{};
    LONG dry_right_{}, dry_left_{}, reverb_right_{}, reverb_left_{};
    std::uint32_t dither_a_{}, dither_b_{};
    void restore_mixer() {
        gnDryROfsVol = dry_right_; gnDryLOfsVol = dry_left_;
        gnRvbROfsVol = reverb_right_; gnRvbLOfsVol = reverb_left_;
        ttp_mod_legacy::dither_a = dither_a_; ttp_mod_legacy::dither_b = dither_b_;
    }
    void save_mixer() {
        dry_right_ = gnDryROfsVol; dry_left_ = gnDryLOfsVol;
        reverb_right_ = gnRvbROfsVol; reverb_left_ = gnRvbLOfsVol;
        dither_a_ = ttp_mod_legacy::dither_a; dither_b_ = ttp_mod_legacy::dither_b;
    }
    template<class T> HRESULT property(T *p, T value) {
        if (!p) return E_POINTER;
        *p = {};
        if (!module_) return E_UNEXPECTED;
        *p = value;
        return S_OK;
    }
public:
    ~ModuleReader() {
        if (module_) { EngineGuard guard; module_.reset(); }
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **p) override {
        if (!p) return E_POINTER;
        if (IsEqualIID(iid, ttp::format::iid_info)) { *p=static_cast<ttp::format::SourceInfo*>(this); AddRef(); return S_OK; }

        if (!p) return E_POINTER; *p = nullptr;
        if (same(iid, IID_IUnknown) || same(iid, iid_reader)) *p = static_cast<Reader*>(this);
        else if (same(iid, iid_metadata)) *p = static_cast<Metadata*>(this);
        else return E_NOINTERFACE;
        AddRef(); return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&refs_); }
    ULONG STDMETHODCALLTYPE Release() override { auto n = InterlockedDecrement(&refs_); if (!n) delete this; return n; }
    HRESULT STDMETHODCALLTYPE Open(IStream* probe_stream, DWORD probe_flags) override {
        const auto probed = PrepareFormatInfo(probe_stream, ttp::format::Module);
        if (FAILED(probed)) return probed;
        return CompleteFormatInfo(OpenImpl(probe_stream, probe_flags));
    }
    HRESULT OpenImpl(IStream *s, DWORD) {
        if (module_) return E_UNEXPECTED;
        auto hr = protect([&] {
            input_ = std::make_unique<Input>(); input_->open(s);
            constexpr uint64_t maximum = 256ull * 1024 * 1024;
            require(input_->size <= maximum, HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE));
            Bytes bytes; bytes.reserve(size_t(input_->size)); BYTE chunk[65536];
            for (;;) {
                require(WaitForSingleObject(input_->event, 0) != WAIT_OBJECT_0, E_ABORT);
                auto got = input_->read(chunk, sizeof(chunk)); if (!got) break;
                require(bytes.size() + got <= maximum, HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE));
                bytes.insert(bytes.end(), chunk, chunk + got);
            }
            require(!bytes.empty());
            EngineGuard guard;
            module_ = std::make_unique<CSoundFile>();
            require(module_->Create(bytes.data(), DWORD(bytes.size())) != FALSE);
            module_->SetRepeatCount(0);
            duration_ = module_->GetLength(FALSE, TRUE);
            maximum_position_ = module_->GetMaxPosition();
            title_ = ansi_title(module_->GetTitle());
            return S_OK;
        });
        if (FAILED(hr)) {
            if (module_) { EngineGuard guard; module_.reset(); }
            input_.reset();
        }
        return hr;
    }
    HRESULT STDMETHODCALLTYPE Capabilities(DWORD *p) override { return property(p, DWORD(2 | (input_ && input_->remote ? 1 : 0))); }
    HRESULT STDMETHODCALLTYPE Duration(DWORD *p) override { return property(p, duration_); }
    HRESULT STDMETHODCALLTYPE Format(WAVEFORMATEX **p) override {
        if (!p) return E_POINTER; *p = nullptr; if (!module_) return E_UNEXPECTED;
        *p = static_cast<WAVEFORMATEX*>(CoTaskMemAlloc(sizeof(format_)));
        if (!*p) return E_OUTOFMEMORY; **p = format_; return S_OK;
    }
    HRESULT STDMETHODCALLTYPE BufferSize(DWORD *p) override { return property(p, DWORD(4096)); }
    HRESULT STDMETHODCALLTYPE CodecName(wchar_t **p) override { return text(L"MOD|Modules Music", p); }
    HRESULT STDMETHODCALLTYPE Bitrate(DWORD *p) override { return property(p, duration_ && input_ ? dword(input_->size * 8000 / duration_) : 0u); }
    HRESULT STDMETHODCALLTYPE BitDepth(WORD *p) override { return property(p, WORD(16)); }
    HRESULT STDMETHODCALLTYPE SetCallback(IUnknown *p) override { return input_ ? input_->callback(p) : E_UNEXPECTED; }
    HRESULT STDMETHODCALLTYPE Start() override { if (!module_) return E_UNEXPECTED; ResetEvent(input_->event); return S_OK; }
    HRESULT STDMETHODCALLTYPE Stop() override { if (!module_) return E_UNEXPECTED; SetEvent(input_->event); return S_OK; }
    HRESULT STDMETHODCALLTYPE Read(Buffer *b) override {
        return protect([&]() -> HRESULT {
            require(b, E_POINTER); check(b->SetLength(0)); require(module_ != nullptr, E_UNEXPECTED);
            require(WaitForSingleObject(input_->event, 0) != WAIT_OBJECT_0, E_ABORT);
            BYTE *p{}; DWORD ignored{}, cap{}; check(b->Data(&p, &ignored)); check(b->Capacity(&cap));
            require(p, E_POINTER); require(cap >= 4, E_INVALIDARG);
            if (pending_offset_ == pending_size_) {
                EngineGuard guard; restore_mixer();
                pending_size_ = module_->Read(pending_.data(), DWORD(pending_.size())) * 4;
                save_mixer(); pending_offset_ = 0;
                require(pending_size_ <= pending_.size());
            }
            DWORD n = std::min(cap / 4 * 4, pending_size_ - pending_offset_);
            if (n) memcpy(p, pending_.data() + pending_offset_, n);
            pending_offset_ += n; check(b->SetLength(n)); return n ? S_OK : S_FALSE;
        });
    }
    HRESULT STDMETHODCALLTYPE Seek(DWORD *p) override {
        return protect([&] {
            require(p, E_POINTER); require(module_ != nullptr, E_UNEXPECTED);
            *p = std::min(*p, duration_);
            EngineGuard guard; restore_mixer();
            module_->SetCurrentPos(duration_ ? DWORD(uint64_t(*p) * maximum_position_ / duration_) : 0);
            save_mixer(); pending_size_ = pending_offset_ = 0;
            ResetEvent(input_->event); return S_OK;
        });
    }
    HRESULT STDMETHODCALLTYPE Count(DWORD *p) override { return property(p, DWORD(1)); }
    HRESULT STDMETHODCALLTYPE At(DWORD i, wchar_t **k, wchar_t **v) override {
        if (!k || !v) return E_POINTER; *k = *v = nullptr; if (i) return E_INVALIDARG;
        auto hr = text(L"Title", k); if (FAILED(hr)) return hr;
        hr = text(title_, v); if (FAILED(hr)) { CoTaskMemFree(*k); *k = nullptr; } return hr;
    }
    HRESULT STDMETHODCALLTYPE Get(const char *k, wchar_t **v) override {
        if (!k || !v) return E_POINTER; *v = nullptr;
        return !_stricmp(k, "Title") ? text(title_, v) : E_INVALIDARG;
    }
    HRESULT STDMETHODCALLTYPE Set(const char*, const wchar_t*) override { return E_NOTIMPL; }
};
HRESULT make_reader(void **p) {
    if (!p) return E_POINTER; *p = nullptr;
    return protect([&] { *p = static_cast<Reader*>(new ModuleReader); return S_OK; });
}
}
