#!/bin/sh

download()
{
	html=$1
	urls=$2
	subdir=${html%.html}_files

	for url in $urls ; do
		echo "Downloading $url... \c"
		mkdir -p "$subdir"
		filename=${url##*/}
		if wget -q -c -O "$subdir/$filename" "$url" ; then
			if [ `uname` = "Darwin" ] ; then
				sed -i '' "s|$url|$subdir/$filename|g" $html
			else
				sed -i "s|$url|$subdir/$filename|g" $html
			fi
			echo "Done."
		else
			echo "Failed."
		fi
	done
}

download_images()
{
	html=$1
	imgs=`grep -io '<img[^>]*' $html | sed 's/<[Ii][Mm][Gg].*[Ss][Rr][Cc]\s*=\s*"*//;s/[" ].*//'`
	download "$html" "$imgs"
}

download_archives()
{
	html=$1
	archives=`grep -ioE 'href\s*=[^>]*(\.zip|\.rar|\.7z|\.tar\.gz|\.tar\.bz2)[^>]*' $html | sed 's/[Hh][Rr][Ee][Ff]\s*=\s*"*//;s/[" ].*//'`
	download "$html" "$archives"
}

if [ $# -eq 0 ] ; then
	echo "Create offline html files."
	echo "Usage: htoff [-i] [-a] htmlfiles"
	echo "       -i  Download images"
	echo "       -a  Download archives"
	exit 0
fi

for arg in "$@" ; do
	if [ $arg = "-i" ] ; then
		need_images="yes"
	elif [ $arg = "-a" ] ; then
		need_archives="yes"
	fi
done

for html in "$@" ; do
	if [ $html = "-i" ] || [ $html = "-a" ] ; then
		continue
	fi

	echo "Processing $html ..."
	cp "$html" "$html.bak"
	if [ "$need_images" = "yes" ] ; then
		download_images $html
	fi
	if [ "$need_archives" = "yes" ] ; then
		download_archives $html
	fi
	echo "Finished processing $html."
done
