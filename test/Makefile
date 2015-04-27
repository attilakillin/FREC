.include <bsd.own.mk>

SUBDIR=	sgrep \
	sgrep-libc \
	sgrep-tre

TEST_PATTERNS=	'printf[^)]*)'

work/hugefile:
	@mkdir -p work
	@${.CURDIR}/scripts/gen_file.sh /usr/src/usr.bin ${.CURDIR}/work/hugefile

test: sgrep sgrep-libc sgrep-tre work/hugefile
.for p in ${TEST_PATTERNS}
	@${.CURDIR}/scripts/gen_output.sh ${p}
	@${.CURDIR}/scripts/benchmark.sh ${p}
.endfor

clean:
	@rm -rf work
	

.include <bsd.subdir.mk>
