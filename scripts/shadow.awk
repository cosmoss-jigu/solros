BEGIN {
        FS=":"
}
{
	if ( $1 == "root" ) {
		# daemon:*:16434:0:99999:7:::
		"openssl passwd -1 -salt phi phi" | getline $2
		print $1 ":" $2 ":" $3 ":" $4 ":" $5 ":" $6 ":" $7 ":" $8 ":" $9
	}
	else {
		print $0
	}
}
