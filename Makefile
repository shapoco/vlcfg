.PHONY: test

TEST_PORT := 52380

test:
	python3 -m http.server $(TEST_PORT) --directory docs &
