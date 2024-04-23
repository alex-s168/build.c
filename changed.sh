doctimestamp=$(date +%s -r "$1")
mkdir -p $(dirname "build/$1.time")
if [ -f "build/$1.time" ]; then
    if [ $doctimestamp -eq $(cat "build/$1.time") ]; then
        exit 0
    fi
fi
echo $doctimestamp > "build/$1.time"
exit 1
