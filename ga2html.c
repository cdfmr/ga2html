#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include "expat.h"

#define XML_FMT_INT_MOD "ll"

int error = 0;
FILE *html = NULL;
int html_index = 0;
int in_title = 0;
int in_time = 0;
int in_content = 0;
int depth = 0;
char *html_prefix = NULL;

static void XMLCALL start_element(void *data, const char *name, const char **atts)
{
	depth++;

	if (!strcmp(name, "entry") && depth == 2)
	{
		if (html)
		{
			fclose(html);
		}

		char *htmlfile = malloc(strlen(html_prefix) + 16);
		sprintf(htmlfile, "%s/%04d.html", html_prefix, ++html_index);
		html = fopen(htmlfile, "wb");
		free(htmlfile);
		if (!html)
		{
			fprintf(stderr, "Can not create %s.\n", htmlfile);
			error = 1;
			return;
		}

		char buf[] = "<html><head><meta content=\"text/html; charset=UTF-8\" "
					 "http-equiv=\"content-type\"></head>";
		fwrite(buf, 1, sizeof(buf) - 1, html);
	}

	else if (html && depth == 3)
	{
		if (!strcmp(name, "published"))
		{
			char buf[] = "<p>";
			fwrite(buf, 1, sizeof(buf) - 1, html);
			in_time = 1;
		}
		
		else if (!strcmp(name, "title"))
		{
			char buf[] = "<h1>";
			fwrite(buf, 1, sizeof(buf) - 1, html);
			in_title = 1;
		}

		else if (!strcmp(name, "content") || !strcmp(name, "summary"))
		{
			char buf[] = "<hr>\n";
			fwrite(buf, 1, sizeof(buf) - 1, html);
			in_content = 1;
		}
		
		else if (!strcmp(name, "link"))
		{
			int i;
			for (i = 0; atts[i]; i += 2)
			{
				if (!strcmp(atts[i], "href"))
				{
					char buf1[] = "<p><a href=\"";
					fwrite(buf1, 1, sizeof(buf1) - 1, html);
					fwrite(atts[i + 1], 1, strlen(atts[i + 1]), html);
					char buf2[] = "\">原文</a></p>";
					fwrite(buf2, 1, sizeof(buf2) - 1, html);
				}
			}
		}
	}
}

static void XMLCALL end_element(void *data, const char *name)
{
	if (!strcmp(name, "entry") && depth == 2)
	{
		if (html)
		{
			char buf[] = "</html>";
			fwrite(buf, 1, sizeof(buf) - 1, html);
			fclose(html);
			html = NULL;
		}
	}

	else if (html && depth == 3)
	{
		if (!strcmp(name, "published"))
		{
			char buf[] = "</p>\n";
			fwrite(buf, 1, sizeof(buf) - 1, html);
			in_time = 0;
		}
		
		else if (!strcmp(name, "title"))
		{
			char buf[] = "</h1>\n";
			fwrite(buf, 1, sizeof(buf) - 1, html);
			in_title = 0;
		}

		else if (!strcmp(name, "content") || !strcmp(name, "summary"))
		{
			in_content = 0;
		}
	}

	depth--;
}

static void XMLCALL handle_data(void *data, const char *content, int length)
{
	if (!html)
	{
		return;
	}
	
	if (in_time || in_title || in_content)
	{
		fwrite(content, 1, length, html);
	}
}

void convert(char *xmlfile)
{
	FILE *xml = fopen(xmlfile, "rb");
	if (!xml)
	{
		fprintf(stderr, "Can not open file %s.\n", xmlfile);
		return;
	}

	XML_Parser parser = XML_ParserCreate(NULL);
	XML_SetUserData(parser, NULL);
	XML_SetElementHandler(parser, start_element, end_element);
	XML_SetCharacterDataHandler(parser, handle_data);

	char buf[BUFSIZ];
	int done;
	error = 0;
	html_index = 0;
	in_time = 0;
	in_title = 0;
	in_content = 0;
	depth = 0;

	int len = (int)strlen(xmlfile);
	html_prefix = (char *)malloc(len + 1);
	strcpy(html_prefix, xmlfile);
	int i;
	for (i = len - 1; i >= 0; i--)
	{
		if (html_prefix[i] == '.')
		{
			html_prefix[i] = 0;
			break;
		}
	}
	mkdir(html_prefix, 0755);
	
	do
	{
		int size = (int)fread(buf, 1, sizeof(buf), xml);
		done = size < sizeof(buf);
		if (XML_Parse(parser, buf, size, done) == XML_STATUS_ERROR)
		{
			fprintf(stderr, "%s at line %lu\n",
					XML_ErrorString(XML_GetErrorCode(parser)),
					(unsigned long)XML_GetCurrentLineNumber(parser));
			fclose(xml);
			return;
		}
	} while (!done && !error);
	
	XML_ParserFree(parser);
	fclose(xml);
	free(html_prefix);
}

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		printf("Usage: ga2html filename.xml\n");
		return 0;
	}

	int i;
	for (i = 1; i < argc; i++)
	{
		convert(argv[i]);
	}

	return 0;
}