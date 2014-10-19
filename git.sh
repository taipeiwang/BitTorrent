git tag -d checkpoint-1
git add git.sh
git add readme.txt
git add tests.txt
git add vulnerabilities.txt
git add bt_parse.c
git add bt_parse.h
git add constants.h
git add chunk.c
git add chunk.h
git add client.c
git add debug-text.h
git add debug.h
git add debug.c
git add input_buffer.c
git add input_buffer.h
git add Makefile
git add peer.c
git add runA.sh
git add runB.sh
git add server.c
git add sha.c
git add sha.h
git add spiffy.c
git add spiffy.h
git add structures.h
git commit -m "commit"
git tag -a checkpoint-1 -m "commit"
git push --tag
