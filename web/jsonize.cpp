
#include <regex>

#include "jsonize.h"

std::string fix_htmlcss_for_web(std::string s)
{
    std::string tmp(s);
    tmp = tmp + "\n\n";
    // We need to fix the link to the WOFF
    tmp = regex_replace(tmp, std::regex("xits-math.woff"), "woff/xits-math.woff");
    // We are not providing the additional stylesheets at the moment, so let us avoid a couple of errors
    tmp = regex_replace(tmp, std::regex("<LINK [^>]*>"), "");
    return tmp;
}

std::vector< std::string > fix_htmldefs_for_web(const std::vector< std::string > &htmldefs) {
    std::vector< std::string > ret;
    for (const auto &elem : htmldefs) {
        ret.push_back(regex_replace(elem, std::regex("SRC='"), "SRC='images/symbols/"));
    }
    return ret;
}

// Support for serializing pairs
/*namespace nlohmann {
    template< typename A, typename B >
    void to_json(json &j, const pair< A, B > &p) {
        j = { p.first, p.second };
    }
}*/

nlohmann::json jsonize(const ExtendedLibraryAddendum &addendum)
{
    nlohmann::json ret;
    ret["htmldefs"] = fix_htmldefs_for_web(addendum.get_htmldefs());
    ret["althtmldefs"] = addendum.get_althtmldefs();
    ret["latexdefs"] = addendum.get_latexdefs();
    ret["htmlcss"] = fix_htmlcss_for_web(addendum.get_htmlcss());
    ret["htmlfont"] = addendum.get_htmlfont();
    ret["htmltitle"] = addendum.get_htmltitle();
    ret["htmlhome"] = addendum.get_htmlhome();
    ret["htmlbibliography"] = addendum.get_htmlbibliography();
    ret["exthtmltitle"] = addendum.get_exthtmltitle();
    ret["exthtmlhome"] = addendum.get_exthtmlhome();
    ret["exthtmllabel"] = addendum.get_exthtmllabel();
    ret["exthtmlbibliography"] = addendum.get_exthtmlbibliography();
    ret["htmlvarcolor"] = addendum.get_htmlvarcolor();
    ret["htmldir"] = addendum.get_htmldir();
    ret["althtmldir"] = addendum.get_althtmldir();
    return ret;
}

nlohmann::json jsonize(const Assertion &assertion)
{
    nlohmann::json ret;
    ret["valid"] = assertion.is_valid();
    ret["theorem"] = assertion.is_theorem();
    ret["usage_disc"] = assertion.is_usage_disc();
    ret["modif_disc"] = assertion.is_modif_disc();
    ret["thesis"] = assertion.get_thesis();
    ret["ess_hyps"] = assertion.get_ess_hyps();
    ret["float_hyps"] = assertion.get_float_hyps();
    ret["opt_hyps"] = assertion.get_opt_hyps();
    ret["dists"] = assertion.get_dists();
    ret["opt_dists"] = assertion.get_opt_dists();
    ret["number"] = assertion.get_number();
    ret["comment"] = assertion.get_comment();
    return ret;
}

nlohmann::json jsonize(const ProofTree<Sentence> &proof_tree) {
    nlohmann::json ret;
    ret["label"] = proof_tree.label;
    ret["sentence"] = proof_tree.sentence;
    ret["children"] = nlohmann::json::array();
    for (const auto &child : proof_tree.children) {
        ret["children"].push_back(jsonize(child));
    }
    ret["dists"] = proof_tree.dists;
    ret["essential"] = proof_tree.essential;
    ret["number"] = proof_tree.number;
    return ret;
}

nlohmann::json jsonize(Step &step)
{
    nlohmann::json ret = nlohmann::json::object();
    ret["id"] = step.get_id();
    ret["children"] = nlohmann::json::array();
    for (const auto &child : step.get_children()) {
        ret["children"].push_back(child.lock()->get_id());
    }
    ret["sentence"] = step.get_sentence();
    ret["did_not_parse"] = step.get_parsing_tree().label == LabTok{};
    bool searching = step.is_searching();
    ret["searching"] = searching;
    if (!searching) {
        auto result = step.get_result();
        ret["found_proof"] = static_cast< bool >(result);
        if (result) {
            ret["proof_data"] = result->get_web_json();
        }
    }
    return ret;
}
