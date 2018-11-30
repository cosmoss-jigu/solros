#!/usr/bin/env python3
import os
import sys
import pdb

CUR_DIR     = os.path.abspath(os.path.dirname(__file__))

class Parser(object):
    def __init__(self):
        self.config = {}   # self.config['SYSTEM'] = 'Linux kernel ...'
        self.data   = {}   # self.data[ self.key ] = {self.schema:VALUE}
        self.key    = ()   # (mem, ext2, DWOM, 0002)
        self.schema = []   # ['ncpu', 'secs', 'works', 'works/sec']

    def parse(self, log_file):
        with open(log_file) as log_fd:
            self.parse2(log_fd)

    def parse2(self, log_fd):
        for l in self._get_line(log_fd):
            parse_fn = self._get_parse_fn(l)
            parse_fn(l)

    def search_data(self, key_list = []):
        results = []
        nkey = self._norm_key(key_list)
        for key in self.data:
            if not self._match_key(nkey, key):
                continue
            rt = (key, self.data[key])
            results.append(rt)
        results.sort()
        return results

    def get_config(self, key):
        return self.config.get(key, None)

    def _match_key(self, key1, key2):
        for (k1, k2) in zip(key1, key2):
            if k1 == "*" or k2 == "*":
                continue
            if k1 != k2:
                return False
        return True

    def _get_line(self, fd):
        for l in fd:
            l = l.strip()
            if l is not "":
                yield(l)

    def _get_parse_fn(self, l):
        type_parser = {
            "###":self._parse_config,
            "##":self._parse_key,
            "#":self._parse_schema,
        }
        return type_parser.get(l.split()[0], self._parse_data)

    def _parse_config(self, l):
        kv = l.split(" ", 1)[1].split("=", 1)
        (key, value) = (kv[0].strip(), kv[1].strip())
        self.config[key] = value

    def _norm_key(self, ks):
        return tuple( map(lambda k: self._norm_str(k), ks))

    def _parse_key(self, l):
        ks = l.split(" ", 1)[1].split(":")
        self.key = self._norm_key(ks)

    def _parse_schema(self, l):
        self.schema = l.split()[1:]

    def _parse_data(self, l):
        for (d_key, d_value) in zip(self.schema, l.split()):
            d_kv = self.data.get(self.key, None)
            if not d_kv:
                self.data[self.key] = d_kv = {}
            d_kv[d_key] = d_value

    def _norm_str(self, s):
        try:
            n = int(s)
            return "%09d" % n
        except ValueError:
            return s

if __name__ == "__main__":
    parser = Parser()
    if len(sys.argv) == 2:
        log_file = sys.argv[1]
        parser.parse(log_file)
    elif len(sys.argv) == 1:
        parser.parse2(sys.stdin)
    else:
        print("Usage: parser.py {fio log file | stdin}")
