#include "cppdecl/declarations/parse.h"
#include "cppdecl/declarations/simplify.h"
#include "cppdecl/declarations/to_string.h"
#include "cppdecl/type_name.h"
#include <emscripten/html5.h>
#include <emscripten.h>
#include <sstream>


std::string escape_xml(std::string_view sv) {
	std::string result;
	result.reserve(sv.size());
	for (size_t i = 0; i < sv.size(); i++) {
		uint8_t c = sv[i];

		if ((c < 32 and c != '\n') or c == 127) {
			continue;
		}
		switch (c) {
		case '<':
			result += "&lt;";
			break;
		case '>':
			result += "&gt;";
			break;
		case '&':
			result += "&amp;";
			break;
		case '\n':
			result += "<br/>\n";
			break;
		case '"':
			result += "&quot;";
			break;
			//case ' ':
			//	result += "&nbsp;";
			break;
		default:
			result += c;
		}
	}
	constexpr std::array<char8_t, 4> lre = {u8"\u202A"}; // Left-to-Right Embedding
	constexpr std::array<char8_t, 4> rle = {u8"\u202B"}; // Right-to-Left Embedding
	constexpr std::array<char8_t, 4> pdf = {u8"\u202C"}; // Pop Directional Formatting
	constexpr std::array<char8_t, 4> lro = {u8"\u202D"}; // Left-to-Right Override
	constexpr std::array<char8_t, 4> rlo = {u8"\u202E"}; // Right-to-Left Override
	constexpr std::array<char8_t, 4> lri = {u8"\u2066"}; // Left-to-Right isolate
	constexpr std::array<char8_t, 4> rli = {u8"\u2067"}; // Right-to-Left isolate
	constexpr std::array<char8_t, 4> fsi = {u8"\u2068"}; // First Strong isolate
	constexpr std::array<char8_t, 4> pdi = {u8"\u2069"}; // Pop Directional isolate

	constexpr std::array<std::array<char8_t, 4>, 9> control = {lre, rle, pdf, lro, rlo, lri, rli, fsi, pdi};
	for (auto& sec : control) {
		auto bidi = std::string_view(reinterpret_cast<const char*>(sec.data()), sec.size() - 1);
		auto pos = result.find(bidi);
		while (pos != std::string::npos) {
			result.replace(pos, sec.size(), "");
			pos = result.find(bidi);
		}
	}
	return result;
}

namespace cppdecl{
void parseText(std::string_view input) {

    auto ret = cppdecl::ParseDecl(input, cppdecl::ParseDeclFlags::accept_everything);
	std::ostringstream out;

        if (auto error = std::get_if<cppdecl::ParseError>(&ret))
        {
            out << "Parse error: " << error->message << '\n';
        }
        else
        {
            if (!input.empty())
                out << "Unparsed junk at the end of input.\n";

            auto &decl = std::get<cppdecl::MaybeAmbiguousDecl>(ret);

            out << "\n--- Parsed to:\n";
            out << cppdecl::ToString(decl, {}) << '\n';

            cppdecl::MaybeAmbiguousDecl simplified_decl = decl;
            cppdecl::Simplify(cppdecl::SimplifyFlags::all, simplified_decl);
            if (simplified_decl != decl)
            {
                out << "\n--- Simplifies to:\n";
                out << cppdecl::ToCode(simplified_decl, {}) << '\n';

                out << "\n--- The simplified version parses to:\n";
                out << cppdecl::ToString(simplified_decl, {}) << '\n';
            }
        }

	auto demangled = out.str();
	auto escaped = escape_xml(demangled);
	escaped = demangled;
	EM_ASM_({ document.getElementById('demangled').value = UTF8ToString($0, $1); }, escaped.data(), escaped.size());
}
}


extern "C" {
EMSCRIPTEN_KEEPALIVE
void parseText(const char* name1, int nlength1) {
	cppdecl::parseText(std::string_view(name1, nlength1));
}
}

void init_loop(void*) {
#ifdef __EMSCRIPTEN__
	emscripten_cancel_main_loop();

	EM_ASM_({
		Module.demangleText = Module.cwrap('demangleText', null, [ 'number', 'number' ]);
		is_emscripten_initialized = 1;
		set_darkmode(darkmode);
		if (pending_demangle) {
			document.getElementById('mangled').value = pending_demangle;
			demangle_text(pending_demangle);
			pending_demangle = null;
		}
		
	});

#endif
}

int main(int, char**) {
	
	emscripten_set_main_loop_arg(init_loop, nullptr, 0, false);
	return 0;
}
