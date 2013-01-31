
"Varnishlog" api ideas
======================


::

        GET /log/100/ -- varnishlog -k 100
        GET /log/100/tag/RxUrl/foo -- varnishlog -k 100 -i RxUrl -I foo
        GET /log/100/tag/RxUrl/foo/bar -- varnishlog -k 100 -i RxUrl -I foo/bar
        GET /log/100/match/"RxUrl:/foo" -- varnishlog -k 100 -m RxUrl:/foo
        GET /log/100/match/RxUrl:foo -- varnishlog -k 100 -m RxUrl:foo
        GET /log/100/match/RxUrl:/foo -- ERRR varnishlog -k 100 -m RxUrl: + garbage /foo
        GET /log/100/match/"RxUrl:/foo\"bar" -- varnishlog -k 100 -m 'RxUrl:/foo"bar'
        GET /log/100/top/RxUrl -- varnishtop -i RxUrl
        GET /log/100/top/RxUrl/foo -- varnishtop -i RxUrl -I /foo



