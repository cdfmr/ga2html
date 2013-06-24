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
#include <sys/stat.h>
#define dir_sep '/'
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
    string style_sheet;
    string link_text;
    string output_dir;
} options;

class conventer
{
public:

    conventer(const string &xml, const options *opt)
        : m_xml(xml)
        , m_options(opt)
        , m_html(NULL)
        , m_index(0)
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
                cout << XML_ErrorString(XML_GetErrorCode(parser))
                     << "at line "
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
    html *m_html;
    state m_state;
    int m_index;

    void write_html()
    {
        char file_index[5];
        sprintf(file_index, "%04d", m_index++);
        string html = m_base + '_' + file_index + ".html";
        if (!m_options->output_dir.empty())
            html = m_options->output_dir + dir_sep + html;
        ofstream output(html.c_str(), ios::out | ios::binary);
        if (!output.is_open()) {
            cout << "Can not open file " << html << endl;
            goto clean;
        }

        output << "<html>";

        output << "<head>";
        output << "<meta content=\"text/html; charset=UTF-8\" "
               << "http-equiv=\"content-type\">";
        output << "<title>" << m_html->title << "</title>";
        if (!m_options->style_sheet.empty()) {
            output << "<link rel=\"stylesheet\" type=\"text/css\" "
                   << "href=\"" << m_options->style_sheet << "\" "
                   << "media=\"all\">";
        }
        output << "</head>";

        output << "<body>";

        output << "<h1>" << m_html->title << "</h1>";
        if (m_options->write_author) {
            output << "<p>";
            output << m_html->author;
            if (!m_html->author.empty())
                output << " @ ";
            replace(m_html->published.begin(), m_html->published.end(),
                    'T', ' ');
            replace(m_html->published.begin(), m_html->published.end(),
                    'Z', ' ');
            output << m_html->published;
            output << "</p>";
        }

        if (m_options->hline_before_content)
            output << "<hr>";
        if (!m_html->content.empty())
            output << m_html->content;
        else if (!m_html->summary.empty())
            output << m_html->summary;
        if (m_options->hline_after_content)
            output << "<hr>";
        if (!m_options->link_text.empty() && !m_html->link.empty()) {
            output << "<a href=\"" << m_html->link << "\">"
                   << m_options->link_text << "</a>";
        }

        output << "</body>";
        output << "</html>";

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

        if (!strcmp(name, "entry") && obj->m_state.depth == 2) {
            if (obj->m_html) delete(obj->m_html);
            obj->m_html = new html;
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
        if (!obj || !obj->m_html) return;

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

void copy_file(const string &source, const string &target)
{
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

    vector<string> files;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-a"))
            opt.write_author = true;
        else if (!strcmp(argv[i], "-hb"))
            opt.hline_before_content = true;
        else if (!strcmp(argv[i], "-ha"))
            opt.hline_after_content = true;
        else if (!strcmp(argv[i], "-s")) {
            if (++i < argc) opt.style_sheet = to_utf8(argv[i]);
        }
        else if (!strcmp(argv[i], "-l")) {
            if (++i < argc) opt.link_text = to_utf8(argv[i]);
        }
        else if (!strcmp(argv[i], "-o")) {
            if (++i < argc) opt.output_dir = argv[i];
        }
        else if (argv[i][0] != '-')
            files.push_back(argv[i]);
    }

    if (files.empty()) {
        cout << "Convert xml files produced by GReader-Archive into html pages"
             << endl << endl;
        cout << "Usage: ga2html [-a] [-hb] [-ha] "
             << "[-s css] [-l link] [-o dir] xmlfiles"
             << endl;
        cout << "-a\tInclude author & publish date in html files" << endl
             << "-hb\tInsert a horizontal line before content" << endl
             << "-ha\tAppend a horizontal line after content" << endl
             << "-s css\tStyle sheet to be used in html files" << endl
             << "-l link\tText for original link" << endl
             << "-o dir\tOutput directory" << endl;
        return -1;
    }

    delete_slash(opt.output_dir);
    if (!create_directory(opt.output_dir)) {
        cout << "Can not create directory " << opt.output_dir << endl;
        return -1;
    }

    if (!opt.style_sheet.empty()) {
        string sheet_file = extract_filename(opt.style_sheet);
        copy_file(opt.style_sheet, opt.output_dir + dir_sep + sheet_file);
        opt.style_sheet = sheet_file;
    }

    for (size_t i = 0; i < files.size(); i++) {
        conventer(files[i], &opt).process();
    }

    return 0;
}
