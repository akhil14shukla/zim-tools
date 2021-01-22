#include "checks.h"
#include "../tools.h"

#include <map>
#include <unordered_map>
#include <list>
#include <sstream>
#define ZIM_PRIVATE
#include <zim/archive.h>
#include <zim/item.h>

// Specialization of std::hash needed for our unordered_map. Can be removed in c++14
namespace std {
  template <> struct hash<LogTag> {
    size_t operator() (const LogTag &t) const { return size_t(t); }
  };
}

// Specialization of std::hash needed for our unordered_map. Can be removed in c++14
namespace std {
  template <> struct hash<TestType> {
    size_t operator() (const TestType &t) const { return size_t(t); }
  };
}

namespace
{

std::unordered_map<LogTag, std::string> tagToStr = {
    {LogTag::ERROR,     "ERROR"},
    {LogTag::WARNING,   "WARNING"}
};

std::unordered_map<TestType, std::pair<LogTag, std::string>> errormapping = {
    { TestType::CHECKSUM,      {LogTag::ERROR, "Invalid checksum"}},
    { TestType::INTEGRITY,     {LogTag::ERROR, "Invalid low-level structure"}},
    { TestType::EMPTY,         {LogTag::ERROR, "Empty articles"}},
    { TestType::METADATA,      {LogTag::ERROR, "Missing metadata entries"}},
    { TestType::FAVICON,       {LogTag::ERROR, "Missing favicon"}},
    { TestType::MAIN_PAGE,     {LogTag::ERROR, "Missing mainpage"}},
    { TestType::REDUNDANT,     {LogTag::WARNING, "Redundant data found"}},
    { TestType::URL_INTERNAL,  {LogTag::ERROR, "Invalid internal links found"}},
    { TestType::URL_EXTERNAL,  {LogTag::ERROR, "Invalid external links found"}},
};

struct MsgInfo
{
  TestType check;
  std::string msgTemplate;
};

std::unordered_map<MsgId, MsgInfo> msgTable = {
  { MsgId::CHECKSUM,         { TestType::CHECKSUM, "ZIM Archive Checksum in archive: {{archive_checksum}}\n" } },
  { MsgId::MAIN_PAGE,        { TestType::MAIN_PAGE, "Main Page Index stored in Archive Header: {{main_page_index}}" } },
  { MsgId::EMPTY_ENTRY,      { TestType::EMPTY, "Entry {{path}} is empty" } },
  { MsgId::OUTOFBOUNDS_LINK, { TestType::URL_INTERNAL, "{{link}} is out of bounds. Article: {{path}}" } },
  { MsgId::EMPTY_LINKS,      { TestType::URL_INTERNAL, "Found {{count}} empty links in article: {{path}}" } },
  { MsgId::DANGLING_LINKS,   { TestType::URL_INTERNAL, "The following links:\n{{links}}({{normalized_link}}) were not found in article {{path}}" } },
  { MsgId::EXTERNAL_LINK,    { TestType::URL_EXTERNAL, "{{link}} is an external dependence in article {{path}}" } },
  { MsgId::REDUNDANT_ITEMS,  { TestType::REDUNDANT, "{{path1}} and {{path2}}" } },
  { MsgId::MISSING_METADATA, { TestType::METADATA, "{{metadata_type}}" } }
};

using kainjow::mustache::mustache;

template<typename T>
std::string toStr(T t)
{
  std::ostringstream ss;
  ss << t;
  return ss.str();
}

std::string escapeJSONString(const std::string& s)
{
  std::string escaped;
  for (char c : s) {
    if (c == '\'' || c == '\\') {
      escaped.push_back('\\');
      escaped.push_back(c);
    } else if ( c == '\n' ) {
      escaped.append("\\n");
    } else {
      escaped.push_back(c);
    }
  }
  return escaped;
}

std::ostream& operator<<(std::ostream& out, const kainjow::mustache::data& d)
{
  assert(d.is_string());
  out << "'" << escapeJSONString(d.string_value()) << "'";
  return out;
}

} // unnamed namespace

ErrorLogger::ErrorLogger(bool _jsonOutputMode)
  : reportMsgs(size_t(TestType::COUNT))
  , jsonOutputMode(_jsonOutputMode)
{
    testStatus.set();
    if ( jsonOutputMode ) {
      std::cout << "{" << std::flush;
    }
}

ErrorLogger::~ErrorLogger()
{
    if ( jsonOutputMode ) {
      std::cout << "\n}" << std::endl;
    }
}

void ErrorLogger::infoMsg(const std::string& msg) const {
  if ( !jsonOutputMode ) {
    std::cout << msg << std::endl;
  }
}

void ErrorLogger::setTestResult(TestType type, bool status) {
    testStatus[size_t(type)] = status;
}

void ErrorLogger::addMsg(MsgId msgid, const MsgParams& msgParams)
{
  const MsgInfo& m = msgTable.at(msgid);
  setTestResult(m.check, false);
  reportMsgs[size_t(m.check)].push_back({msgid, msgParams});

}

std::string ErrorLogger::expand(const MsgIdWithParams& msg)
{
  const MsgInfo& m = msgTable.at(msg.msgId);
  mustache tmpl{m.msgTemplate};
  return tmpl.render(msg.msgParams);
}

void ErrorLogger::jsonOutput(const MsgIdWithParams& msg) const {
  const MsgInfo& m = msgTable.at(msg.msgId);
  const std::string i = indentation();
  const std::string i2 = i + i;
  const std::string i3 = i2 + i;
  std::cout << i2 << "{\n";
  std::cout << i3 << "'check' : " << formatForJSON(m.check) << ",\n";
  std::cout << i3 << "'level' : '" << tagToStr[errormapping[m.check].first] << "',\n";
  std::cout << i3 << "'code' : " << size_t(msg.msgId) << ",\n"; // XXX
  std::cout << i3 << "'message' : '" << escapeJSONString(expand(msg)) << "'";
  for ( const auto& kv : msg.msgParams ) {
    std::cout << ",\n" << i3 << "'" << kv.first << "' : " << kv.second;
  }
  std::cout << "\n" << i2 << "}";
}

void ErrorLogger::report(bool error_details) const {
    if ( !jsonOutputMode ) {
        for ( size_t i = 0; i < size_t(TestType::COUNT); ++i ) {
            const auto& testmsg = reportMsgs[i];
            if ( !testStatus[i] ) {
                auto &p = errormapping[TestType(i)];
                std::cout << "[" + tagToStr[p.first] + "] " << p.second << ":" << std::endl;
                for (auto& msg: testmsg) {
                    std::cout << "  " << expand(msg) << std::endl;
                }
            }
        }
    } else {
        std::cout << sep << indentation() << "'logs' : [";
        const char* msgSep = "\n";
        for ( size_t i = 0; i < size_t(TestType::COUNT); ++i ) {
            for (const auto& msg: reportMsgs[i]) {
                std::cout << msgSep;
                jsonOutput(msg);
                msgSep = ",\n";
            }
        }
        std::cout << "\n" << indentation() << "]";
    }
}

bool ErrorLogger::overallStatus() const {
    for ( size_t i = 0; i < size_t(TestType::COUNT); ++i ) {
        if (errormapping[TestType(i)].first == LogTag::ERROR) {
            if ( testStatus[i] == false ) {
                return false;
            }
        }
    }
    return true;
}


void test_checksum(zim::Archive& archive, ErrorLogger& reporter) {
    reporter.infoMsg("[INFO] Verifying Internal Checksum...");
    bool result = archive.check();
    if (!result) {
        reporter.infoMsg("  [ERROR] Wrong Checksum in ZIM archive");
        reporter.addMsg(MsgId::CHECKSUM, {{"archive_checksum", archive.getChecksum()}});
    }
}

void test_integrity(const std::string& filename, ErrorLogger& reporter) {
    reporter.infoMsg("[INFO] Verifying ZIM-archive structure integrity...");
    zim::IntegrityCheckList checks;
    checks.set(); // enable all checks (including checksum)
    bool result = zim::validate(filename, checks);
    reporter.setTestResult(TestType::INTEGRITY, result);
    if (!result) {
        reporter.infoMsg("  [ERROR] ZIM file's low level structure is invalid");
    }
}


void test_metadata(const zim::Archive& archive, ErrorLogger& reporter) {
    reporter.infoMsg("[INFO] Searching for metadata entries...");
    static const char* const test_meta[] = {
        "Title",
        "Creator",
        "Publisher",
        "Date",
        "Description",
        "Language"};
    auto existing_metadata = archive.getMetadataKeys();
    auto begin = existing_metadata.begin();
    auto end = existing_metadata.end();
    for (auto &meta : test_meta) {
        if (std::find(begin, end, meta) == end) {
            reporter.addMsg(MsgId::MISSING_METADATA, {{"metadata_type", meta}});
        }
    }
}

void test_favicon(const zim::Archive& archive, ErrorLogger& reporter) {
    reporter.infoMsg("[INFO] Searching for Favicon...");
    static const char* const favicon_paths[] = {"-/favicon.png", "I/favicon.png", "I/favicon", "-/favicon"};
    for (auto &path: favicon_paths) {
        if (archive.hasEntryByPath(path)) {
            return;
        }
    }
    reporter.setTestResult(TestType::FAVICON, false);
}

void test_mainpage(const zim::Archive& archive, ErrorLogger& reporter) {
    reporter.infoMsg("[INFO] Searching for main page...");
    bool testok = true;
    try {
      archive.getMainEntry();
    } catch(...) {
        testok = false;
    }
    if (!testok) {
        reporter.addMsg(MsgId::MAIN_PAGE, {{"main_page_index", toStr(archive.getMainEntryIndex())}});
    }
}


void test_articles(const zim::Archive& archive, ErrorLogger& reporter, ProgressBar progress,
                   const EnabledTests checks) {
    reporter.infoMsg("[INFO] Verifying Articles' content...");
    // Article are store in a map<hash, list<index>>.
    // So all article with the same hash will be stored in the same list.
    std::map<unsigned int, std::list<zim::entry_index_type>> hash_main;

    int previousIndex = -1;

    progress.reset(archive.getEntryCount());
    for (auto& entry:archive.iterEfficient()) {
        progress.report();
        auto path = entry.getPath();
        char ns = archive.hasNewNamespaceScheme() ? 'C' : path[0];

        if (entry.isRedirect() || ns == 'M') {
            continue;
        }

        if (checks.isEnabled(TestType::EMPTY) && (ns == 'C' || ns=='A' || ns == 'I')) {
            auto item = entry.getItem();
            if (item.getSize() == 0) {
                reporter.addMsg(MsgId::EMPTY_ENTRY, {{"path", path}});
            }
        }

        auto item = entry.getItem();

        if (item.getSize() == 0) {
            continue;
        }

        std::string data;
        if (checks.isEnabled(TestType::REDUNDANT) || item.getMimetype() == "text/html")
            data = item.getData();

        if(checks.isEnabled(TestType::REDUNDANT))
            hash_main[adler32(data)].push_back( item.getIndex() );

        if (item.getMimetype() != "text/html")
            continue;

        std::vector<html_link> links;
        if (checks.isEnabled(TestType::URL_INTERNAL) ||
            checks.isEnabled(TestType::URL_EXTERNAL)) {
            links = generic_getLinks(data);
        }

        if(checks.isEnabled(TestType::URL_INTERNAL))
        {
            auto baseUrl = path;
            auto pos = baseUrl.find_last_of('/');
            baseUrl.resize( pos==baseUrl.npos ? 0 : pos );

            std::unordered_map<std::string, std::vector<std::string>> filtered;
            int nremptylinks = 0;
            for (const auto &l : links)
            {
                if (l.link.front() == '#' || l.link.front() == '?') continue;
                if (l.isInternalUrl() == false) continue;
                if (l.link.empty())
                {
                    nremptylinks++;
                    continue;
                }


                if (isOutofBounds(l.link, baseUrl))
                {
                    reporter.addMsg(MsgId::OUTOFBOUNDS_LINK, {{"link", l.link}, {"path", path}});
                    continue;
                }

                auto normalized = normalize_link(l.link, baseUrl);
                filtered[normalized].push_back(l.link);
            }

            if (nremptylinks)
            {
                reporter.addMsg(MsgId::EMPTY_LINKS, {{"count", toStr(nremptylinks)}, {"path", path}});
            }

            for(const auto &p: filtered)
            {
                const std::string link = p.first;
                if (!archive.hasEntryByPath(link)) {
                    int index = item.getIndex();
                    if (previousIndex != index)
                    {
                        std::string links;
                        for (const auto &olink : p.second)
                            links += "- " + olink + "\n";
                        reporter.addMsg(MsgId::DANGLING_LINKS, {{"path", path}, {"normalized_link", link}, {"links", links}});
                        previousIndex = index;
                    }
                    reporter.setTestResult(TestType::URL_INTERNAL, false);
                }
            }
        }

        if (checks.isEnabled(TestType::URL_EXTERNAL))
        {
            for (const auto &l: links)
            {
                if (l.attribute == "src" && l.isExternalUrl())
                {
                    reporter.addMsg(MsgId::EXTERNAL_LINK, {{"link", l.link}, {"path", path}});
                    break;
                }
            }
        }
    }

    if (checks.isEnabled(TestType::REDUNDANT))
    {
        reporter.infoMsg("[INFO] Searching for redundant articles...");
        reporter.infoMsg("  Verifying Similar Articles for redundancies...");
        std::ostringstream output_details;
        progress.reset(hash_main.size());
        for(const auto &it: hash_main)
        {
            progress.report();
            auto l = it.second;
            while ( !l.empty() ) {
                const auto e1 = archive.getEntryByPath(l.front());
                l.pop_front();
                if ( !l.empty() ) {
                    // The way we have constructed `l`, e1 MUST BE an item
                    const std::string s1 = e1.getItem().getData();
                    decltype(l) articlesDifferentFromE1;
                    for(auto other : l) {
                        auto e2 = archive.getEntryByPath(other);
                        std::string s2 = e2.getItem().getData();
                        if (s1 != s2 ) {
                            articlesDifferentFromE1.push_back(other);
                            continue;
                        }

                        reporter.addMsg(MsgId::REDUNDANT_ITEMS, {{"path1", e1.getPath()}, {"path2", e2.getPath()}});
                    }
                    l.swap(articlesDifferentFromE1);
                }
            }
        }
    }
}
