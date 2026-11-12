# Common tasks. Run `make help` to list them.
.PHONY: help test test-firmware test-web demo

help:
	@echo "make test           run every test (firmware simulation + web server)"
	@echo "make test-firmware  run the firmware's safety tests on this PC (needs g++ and make)"
	@echo "make test-web       run the web server tests (needs Python 3 and pytest)"
	@echo "make demo           start the dashboard with a simulated machine"

test: test-firmware test-web

test-firmware:
	$(MAKE) -C tests/firmware test

test-web:
	python3 -m pytest tests/web -q

demo:
	python3 web/server.py --demo
