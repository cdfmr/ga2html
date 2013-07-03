#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cassert>
#include <cstring>
#include "expat.h"

#ifdef __WIN32
#include <windows.h>
#define dir_sep '\\'
#else
#include <unistd.h>
#include <sys/stat.h>
#define dir_sep '/'
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

using namespace std;

#define XML_FMT_INT_MOD "ll"

typedef struct tag_html
{
    string author;
    string published;
    string link;
    string title;
    string summary;
    string content;
} html;

typedef struct tag_state
{
    int depth;
    bool in_root_title;
    bool in_author;
    bool in_published;
    bool in_title;
    bool in_summary;
    bool in_content;
} state;

typedef struct tag_options
{
    bool write_author;
    bool hline_before_content;
    bool hline_after_content;
    int article_count;
    string link_text;
    string output_dir;
    string time_prefix;
} options;

class conventer
{
public:

    conventer(const string &xml, const options *opt)
        : m_xml(xml)
        , m_options(opt)
        , m_html(NULL)
        , m_index(0)
        , m_count(0)
    {
        assert(!xml.empty() && opt);
        memset(&m_state, 0, sizeof(m_state));

        m_base = xml;
        size_t pos = m_base.find_last_of('.');
        if (pos != string::npos) m_base.resize(pos);
        pos = m_base.find_last_of(dir_sep);
        if (pos != string::npos) m_base.erase(0, pos + 1);
    }

    ~conventer()
    {
        close_html();
        delete(m_html);
    }

    void process()
    {
        ifstream input(m_xml.c_str(), ios::in | ios::binary);
        if (!input.is_open()) {
            cout << "Can not open file " << m_xml << endl;
            return;
        }

        XML_Parser parser = XML_ParserCreate(NULL);
        XML_SetUserData(parser, this);
        XML_SetElementHandler(parser, start_element, end_element);
        XML_SetCharacterDataHandler(parser, handle_data);

        int done;
        do {
            char buffer[BUFSIZ];
            input.read(buffer, sizeof(buffer));
            done = input.eof();
            if (XML_Parse(parser, buffer, input.gcount(), done)
                == XML_STATUS_ERROR) {
                cout << m_xml << ": "
                     << XML_ErrorString(XML_GetErrorCode(parser))
                     << " at line "
                     << XML_GetCurrentLineNumber(parser)
                     << endl;
                break;
            }
        } while (!done);

        XML_ParserFree(parser);
    }

private:

    string m_xml;
    string m_base;
    const options *m_options;
    string m_title;
    html *m_html;
    state m_state;
    ofstream m_output;
    int m_index;
    int m_count;

    bool open_html()
    {
        string html = m_base;
        if (!m_options->time_prefix.empty())
            html = html + '_' + m_options->time_prefix;
        if (m_options->article_count != 0) {
            char file_index[5];
            sprintf(file_index, "%04d", m_index++);
            html = html + '_' + file_index;
        }
        html.append(".html");
        if (!m_options->output_dir.empty())
            html = m_options->output_dir + dir_sep + html;
        m_output.open(html.c_str(), ios::out | ios::binary);
        if (!m_output.is_open()) {
            cout << "Can not open file " << html << endl;
            return false;
        }

        m_output << "<html>" << endl;
        m_output << "<head>" << endl;
        m_output << "<meta content=\"text/html; charset=UTF-8\" "
                 << "http-equiv=\"content-type\">"
                 << endl;
        m_output << "<link rel=\"stylesheet\" type=\"text/css\" "
                 << "href=\"style.css\" media=\"all\">"
                 << endl;
        m_output << "<script type=\"text/javascript\" src=\"jquery.js\">"
                 << "</script>"
                 << endl;
        m_output << "<script type=\"text/javascript\">"          << endl
                 << "$(document).ready(function() {"             << endl
                 << "  $(\".msg_body\").hide();"                 << endl
                 << "  $(\".msg_head\").click(function() {"      << endl
                 << "    var interval = 250;"                    << endl
                 << "    var msg = $(this).next(\".msg_body\");" << endl
                 << "    if (msg.is(\":hidden\")) {"             << endl
//               << "      $(\".msg_body\").hide();"             << endl
                 << "      msg.slideDown(interval);"             << endl
                 << "      $(\"html,body\").animate({scrollTop: "
                 <<          "$(this).offset().top}, interval);" << endl
                 << "    } else {"                               << endl
                 << "      msg.slideUp(interval);"               << endl
                 << "    }"                                      << endl
                 << "  });"                                      << endl
                 << "});"                                        << endl
                 << "</script>"                                  << endl;
        string title = m_options->article_count == 1 ? m_html->title :
                       m_options->time_prefix.empty() ? m_title :
                       m_title + " " + m_options->time_prefix;
        m_output << "<title>" << title << "</title>" << endl;
        m_output << "</head>" << endl;
        m_output << "<body>" << endl;
        if (m_options->article_count != 1) {
            m_output << "<div align=\"center\"><h1><strong>"
                     << title
                     << "</strong></h1></div>" << endl;
            m_output << "<div class=\"msg_list\">" << endl;
        }
        return true;
    }

    void close_html()
    {
        if (m_output.is_open()) {
            if (m_options->article_count != 1)
                m_output << "</div>" << endl;
            m_output << "</body>" << endl;
            m_output << "</html>" << endl;
            m_output.close();
            m_count = 0;
        }
    }

    void write_html()
    {
        if (!m_options->time_prefix.empty() &&
            m_html->published.find(m_options->time_prefix) != 0)
            goto clean;

        if (!m_output.is_open() && !open_html())
            goto clean;

        if (m_options->article_count != 1) {
            m_output << "<p class=\"msg_head\">◇ "
                     << m_html->title
                     << "</p>" << endl;
            m_output << "<div class=\"msg_body\">" << endl;
        }

        m_output << "<h1>" << m_html->title << "</h1>";
        if (m_options->write_author) {
            m_output << "<p>";
            m_output << m_html->author;
            if (!m_html->author.empty())
                m_output << " @ ";
            replace(m_html->published.begin(), m_html->published.end(),
                    'T', ' ');
            replace(m_html->published.begin(), m_html->published.end(),
                    'Z', ' ');
            m_output << m_html->published;
            m_output << "</p>";
            m_output << endl;
        }

        if (m_options->hline_before_content)
            m_output << "<hr>" << endl;
        if (!m_html->content.empty())
            m_output << m_html->content;
        else if (!m_html->summary.empty())
            m_output << m_html->summary;
        m_output << endl;
        if (m_options->hline_after_content)
            m_output << "<hr>" << endl;
        if (!m_options->link_text.empty() && !m_html->link.empty()) {
            m_output << "<p><a href=\"" << m_html->link << "\">"
                     << m_options->link_text << "</a></p>" << endl;
        }

        if (m_options->article_count != 1)
            m_output << "</div>" << endl;

        if (++m_count == m_options->article_count)
            close_html();

    clean:
        delete(m_html);
        m_html = NULL;
        int depth = m_state.depth;
        memset(&m_state, 0, sizeof(m_state));
        m_state.depth = depth;
    }

    static void XMLCALL start_element(void *data,
                                      const char *name,
                                      const char **atts)
    {
        conventer *obj = (conventer *)data;
        if (!obj) return;

        obj->m_state.depth++;

        if (obj->m_state.depth == 2) {
            if (!strcmp(name, "title"))
                obj->m_state.in_root_title = true;
            else if (!strcmp(name, "entry")) {
                if (obj->m_html) delete(obj->m_html);
                obj->m_html = new html;
            }
        }

        if (!obj->m_html) return;

        if (obj->m_state.depth == 3) {
            if (!strcmp(name, "published"))
                obj->m_state.in_published = true;
            else if (!strcmp(name, "title"))
                obj->m_state.in_title = true;
            else if (!strcmp(name, "summary"))
                obj->m_state.in_summary = true;
            else if (!strcmp(name, "content"))
                obj->m_state.in_content = true;
            else if (!strcmp(name, "link")) {
                for (int i = 0; atts[i] && atts[i + 1]; i += 2) {
                    if (!strcmp(atts[i], "href")) {
                        obj->m_html->link = atts[i + 1];
                    }
                }
            }
        }

        else if (obj->m_state.depth == 4 && !strcmp(name, "name"))
            obj->m_state.in_author = true;
    }

    static void XMLCALL end_element(void *data, const char *name)
    {
        conventer *obj = (conventer *)data;
        if (!obj) return;

        if (!strcmp(name, "title") && obj->m_state.depth == 2) {
            obj->m_state.in_root_title = false;
            obj->m_state.depth--;
            return;
        }

        if (!obj->m_html) {
            obj->m_state.depth--;
            return;
        }

        if (!strcmp(name, "entry") && obj->m_state.depth == 2)
            obj->write_html();

        else if (obj->m_state.depth == 3) {
            if (!strcmp(name, "published"))
                obj->m_state.in_published = false;
            else if (!strcmp(name, "title"))
                obj->m_state.in_title = false;
            else if (!strcmp(name, "summary"))
                obj->m_state.in_summary = false;
            else if (!strcmp(name, "content"))
                obj->m_state.in_content = false;
        }

        else if (obj->m_state.depth == 4 && !strcmp(name, "name"))
            obj->m_state.in_author = false;

        obj->m_state.depth--;
    }

    static void XMLCALL handle_data(void *data, const char *content, int length)
    {
        conventer *obj = (conventer *)data;
        if (!obj) return;

        if (obj->m_state.in_root_title)
            obj->m_title.append(content, length);
        else if (obj->m_html) {
            if (obj->m_state.in_author)
                obj->m_html->author.append(content, length);
            else if (obj->m_state.in_published)
                obj->m_html->published.append(content, length);
            else if (obj->m_state.in_title)
                obj->m_html->title.append(content, length);
            else if (obj->m_state.in_summary)
                obj->m_html->summary.append(content, length);
            else if (obj->m_state.in_content)
                obj->m_html->content.append(content, length);
        }
    }
};

#ifdef __WIN32
string to_utf8(const char *text)
{
    int wsize = MultiByteToWideChar(CP_ACP, 0, text, strlen(text), NULL, 0);
    wchar_t *wtext = (wchar_t *)malloc(sizeof(wchar_t) * (wsize));
    MultiByteToWideChar(CP_ACP, 0, text, strlen(text), wtext, wsize);

    int usize = WideCharToMultiByte(CP_UTF8, 0, wtext, wsize, NULL, 0, 0, 0);
    char *utext = (char *)malloc(usize + 1);
    WideCharToMultiByte(CP_UTF8, 0, wtext, wsize, utext, usize, 0, 0);
    utext[usize] = 0;

    return string(utext);
}
#else
#define to_utf8(x) (x)
#endif

void delete_slash(string &dir)
{
#ifdef __WIN32
    if (!dir.empty() && dir[dir.size() - 1] == dir_sep)
#else
    while (!dir.empty() && dir[dir.size() - 1] == dir_sep)
#endif
        dir.resize(dir.size() - 1);
}

string extract_path(const string &filename)
{
    int pos = filename.find_last_of(dir_sep);
    if (pos != string::npos)
        return filename.substr(0, pos);
    return string("");
}

string extract_filename(const string &filename)
{
    int pos = filename.find_last_of(dir_sep);
    if (pos != string::npos)
        return filename.substr(pos + 1);
    return filename;
}

bool directory_exists(const string &dir)
{
#ifdef __WIN32
    DWORD code = GetFileAttributes(dir.c_str());
    return (code != -1) && (code & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    memset(&st, 0, sizeof(st));
    stat(dir.c_str(), &st);
    return (st.st_mode & S_IFDIR);
#endif
}

#ifdef __WIN32
#define mkdir(d, p) (CreateDirectory((d), 0) ? 0 : -1)
#endif

bool create_directory(const string &path)
{
    string dir = path;
    delete_slash(dir);

#ifdef __WIN32
    if (dir.size() == 2 && dir[1] == ':')
        return true;
#endif
    if (dir.empty() || directory_exists(dir))
        return true;

    return create_directory(extract_path(dir)) &&
           mkdir(dir.c_str(), 0755) == 0;
}

string get_bin_path()
{
    string bin_path;

    char buffer[1024];
#ifdef __WIN32
    if (GetModuleFileName(0, buffer, sizeof(buffer)) != 0)
#elif defined __APPLE__
    uint32_t size = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &size) == 0)
#else
    char proc_exe[32];
    sprintf(proc_exe, "/proc/%d/exe", getpid());
    if (readlink(proc_exe, buffer, sizeof(buffer)) != -1)
#endif
        bin_path = extract_path(string(buffer));

    return bin_path;
}

void copy_file(const string &source, const string &target)
{
    if (source == target) return;

#ifdef __WIN32
    CopyFile(source.c_str(), target.c_str(), FALSE);
#else
    ifstream input(source.c_str(), ios::in | ios::binary);
    ofstream output(target.c_str(), ios::out | ios::binary);
    if (input.is_open() && output.is_open()) {
        do {
            char buffer[BUFSIZ];
            input.read(buffer, sizeof(buffer));
            output.write(buffer, input.gcount());
        } while (!input.eof());
    }
#endif
}

int main(int argc, char *argv[])
{
    options opt;
    opt.write_author = false;
    opt.hline_before_content = false;
    opt.hline_after_content = false;
    opt.article_count = 1;

    bool error = false;
    vector<string> files;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-a"))
            opt.write_author = true;
        else if (!strcmp(argv[i], "-hb"))
            opt.hline_before_content = true;
        else if (!strcmp(argv[i], "-ha"))
            opt.hline_after_content = true;
        else if (!strcmp(argv[i], "-n")) {
            if (++i < argc) opt.article_count = atoi(argv[i]);
            if (opt.article_count < 0) error = true;
        }
        else if (!strcmp(argv[i], "-l")) {
            if (++i < argc) opt.link_text = to_utf8(argv[i]);
        }
        else if (!strcmp(argv[i], "-o")) {
            if (++i < argc) opt.output_dir = argv[i];
        }
        else if (!strcmp(argv[i], "-t")) {
            if (++i < argc) opt.time_prefix = argv[i];
        }
        else if (argv[i][0] != '-')
            files.push_back(argv[i]);
        else
            error = true;
    }

    if (error || files.empty()) {
        cout << "Convert xml files produced by GReader-Archive into html pages"
             << endl << endl;
        cout << "Usage: ga2html [-a] [-hb] [-ha] "
             << "[-n num] [-l link] [-o dir] xmlfiles"
             << endl;
        cout << "-a\tInclude author & publish date in html files" << endl
             << "-hb\tInsert a horizontal line before content" << endl
             << "-ha\tAppend a horizontal line after content" << endl
             << "-n num\tArticle count of per html file, 0 for all" << endl
             << "-l link\tText for original link" << endl
             << "-o dir\tOutput directory" << endl;
        return -1;
    }

    delete_slash(opt.output_dir);
    if (!create_directory(opt.output_dir)) {
        cout << "Can not create directory " << opt.output_dir << endl;
        return -1;
    }

    string bin_path = get_bin_path();
    if (bin_path.empty()) bin_path = extract_path(string(argv[0]));
    if (!bin_path.empty()) bin_path += dir_sep;
    string out_path = opt.output_dir;
    if (!out_path.empty()) out_path += dir_sep;
    copy_file(bin_path + "style.css", out_path + "style.css");
    copy_file(bin_path + "jquery.js", out_path + "jquery.js");

    for (size_t i = 0; i < files.size(); i++) {
        cout << "Converting " << files[i] << "...";
        conventer(files[i], &opt).process();
        cout << " Done." << endl;
    }

    return 0;
}
