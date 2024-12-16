run:
	odin strip-semicolon game -no-entry-point
	odin build game -debug -o:none -build-mode:dll -out:bin/game.dll

	odin build main.odin -file -debug -o:none  -out:bin/entry
	./bin/entry
