{ pkgs }: {
	deps = [
   pkgs.ack
   pkgs.curlFull
		pkgs.clang_12
		pkgs.ccls
		pkgs.gdb
		pkgs.gnumake
	];
}