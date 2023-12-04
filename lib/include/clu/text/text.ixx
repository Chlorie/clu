module;
#include <clu/macros.h>
#include <clu/text.h>

export module clu.text;

export namespace clu
{
    using clu::print_nonformatted;
    using clu::vprint;
    using clu::print;
    using clu::println;

    using clu::decoding_errc;
    using clu::make_error_code;
    using clu::utf_encode_result;
    using clu::utf8_encode_result;
    using clu::utf16_encode_result;
    using clu::decode_one;
    using clu::encode_one_utf16;
    using clu::to_wstring;
} // namespace clu

// WORKAROUND: If this is not exported, anything related to std::format breaks
#if defined(CLU_MSVC) && _MSC_FULL_VER <= 193833130
#pragma warning(push)
#pragma warning(disable : 4455)
export namespace std
{
    using std::operator""sv;
} // namespace std
#pragma warning(pop)
#endif
