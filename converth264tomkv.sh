#/bin/sh
for x in *.h264; do
        if [[ `lsof | grep $x` ]]
        then
                echo "file $x is open by some program, skipping"
                continue
        fi
        echo "file $x is not open, converting..."
       ffmpeg -y -framerate 15 -i $x -c copy $x.mkv
       [ $(stat --printf '%s' "$x.mkv") -gt $(stat --printf '%s' "$x") ] && rm $x || echo "$x.mkv is less than $x"
done
