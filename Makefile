all: etags
	python build_analyzer.py

etags:
	etags source/*
