
# @author Lorenzo Arcidiacono 534235
# Si dichiara che il programma è in ogni sua parte opera originale dell'autore.

#!/bin/bash

#controllo se l'opzione -help è settata
for OPT; do
    if [ $OPT = "-help" ]; then
        echo "usage: $0 [-help] conf_file time2" 1>&2
        exit 1
    fi
done

#controllo che gli argomenti siano 2
if ! [ $# = 2 ] ; then
    echo "usage: $0 [-help] conf_file time2" 1>&2
    exit 1
fi

#controllo che il file esista e non sia speciale
if ! [ -f $1 ]; then
    echo "Errore: $1 file speciale o non esistente" 1>&2
    echo "usage: $0 [-help] conf_file time2" 1>&2
    exit 1
fi

#cerco ed estraggo il path della directory specificata nel file

DIR=$(grep -v '#' $1 | grep DirName | cut -f 2 -d "=")
DIR=$(echo $DIR | tr -d ' ')

echo "dir: $DIR"

if ! [ -d $DIR ]; then
    echo "usage: $0 [-help] conf_file time2" 1>&2
    exit 1
fi


if [ $2 -lt 0 ]; then
	echo " Errore: time minore di 0" 1>&2
	echo "usage: $0 [-help] conf_file time2" 1>&2
    exit 1
fi


FILES=$DIR/*


if [ $2 = 0 ]; then
    for File in $FILES; do
        echo $File
    done

else
    find $DIR -mmin +$2 ! -path $DIR -exec tar -cvzf chatty.tar.gz {} + | xargs rm -vfd
fi

echo "Script terminato correttamente"
exit 0

#| xargs rm  -vfd  

