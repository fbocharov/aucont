# function build_target {
# 	cd "$1"
# 	printf "building %s... " "$(basename "$1")"
# 	make 1>/dev/null
# 	if [ $? -ne "0" ]; then
# 		printf "FAILED\n"
# 	else
# 		printf "OK\n"
# 	fi
# 	cd ..
# }

# cd /test/aucont
mkdir -p bin
cd src


cd libaucont
printf "building %s... " "$(basename libaucont)"
make 1>/dev/null
if [ $? -ne "0" ]; then
	printf "FAILED\n"
else
	printf "OK\n"
fi
cd ..


for d in aucont_*/; do
	cd "$d"
	printf "building %s... " "$(basename "$d")"
	make 1>/dev/null
	if [ $? -ne "0" ]; then
		printf "FAILED\n"
	else
		printf "OK\n"
	fi
	cd ..



	# build_target "$d"
done
