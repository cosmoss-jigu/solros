#!/usr/bin/env python3
import os
import stat
import sys
import subprocess
import optparse
import math
import pdb
from parser import Parser

CUR_DIR     = os.path.abspath(os.path.dirname(__file__))

"""
# GNUPLOT HOWTO
- http://www.gnuplotting.org/multiplot-placing-graphs-next-to-each-other/
- http://stackoverflow.com/questions/10397750/embedding-multiple-datasets-in-a-gnuplot-command-script
- http://ask.xmodulo.com/draw-stacked-histogram-gnuplot.html
"""

class Plotter(object):
    def __init__(self, log_file):
        # config
        self.UNIT_WIDTH  = 2.3
        self.UNIT_HEIGHT = 2.3
        self.PAPER_WIDTH = 7   # USENIX text block width
        self.UNIT = 1000000.0

        # init.
        self.log_file = log_file
        self.parser = Parser()
        self.parser.parse(self.log_file)
        self.out_dir  = ""
        self.out_file = ""
        self.out = 0

    def _setup_output(self, out_dir):
        self.out_dir  = out_dir
        subprocess.call("mkdir -p %s" % self.out_dir, shell=True)
        self.out_file = os.path.join(self.out_dir, "plot.gp")
        self.out = open(self.out_file, "w")

    def _wrapup_output(self):
        self.out.close()

    def plot_bw_iops(self, workload, out_dir):
        self._setup_output(out_dir)
        self._gen_data(workload)
        self._plot_header()
        self._plot_bw(workload)
        self._plot_iops(workload)
        self._plot_footer()
        self._gen_pdf(self.out_file)
        self._wrapup_output()

    def _get_ncores(self, workload):
        data = self.parser.search_data([workload, "*", "*"])
        ncores_set = set()
        for kd in data:
            ncores_set.add( int(kd[0][2]) )
        return sorted( list(ncores_set) )

    def _get_data_file(self, workload, ncore):
        return "%s:-:%s.dat" % (workload, ncore)

    def _conv_tic(self, tic_tbl, num):
        n = float(num)
        for tic in tic_tbl:
            nn = n/tic[1]
            if int(nn) == nn:
                nn_str = "%d%s" % (int(nn), tic[0])
            else:
                nn_str = "%.2f%s" % (nn, tic[0])
            if nn >= 1.0:
                return nn_str
        return nn_str

    def _conv_byte_tic(self, num):
        byte_tic = [("TB", 1024.0**4),
                    ("GB", 1024.0**3),
                    ("MB", 1024.0**2),
                    ("KB", 1024.0**1),
                    ("B",  1024.0**0)]
        return self._conv_tic(byte_tic, num)

    def _conv_iops_tic(self, num):
        iops_tic = [("T", 1000.0**4),
                    ("G", 1000.0**3),
                    ("M", 1000.0**2),
                    ("K", 1000.0**1),
                    ("",  1000.0**0)]
        return self._conv_tic(iops_tic, num)

    def _get_pretty_workload(self, workload):
        tbl = {"read":"sequential read",
               "randread":"random read"}
        return tbl.get(workload, workload)

    def _gen_data(self, workload):
        ncores = self._get_ncores(workload)
        for ncore in ncores:
            data = self.parser.search_data([workload, "*",  ncore])
            if data == []:
                continue
            data_file = os.path.join(self.out_dir,
                                     self._get_data_file(workload, ncore))
            with open(data_file, "w") as out:
                print("# %s:*:%s" % (workload, ncore), file=out)
                results = []
                for d_kv in data:
                    # bs bw iops
                    d_kv = d_kv[1]
                    (bs, bw, iops) = (float(d_kv["bs"]),
                                      float(d_kv["bw"]),
                                      float(d_kv["iops"]) )
                    (bs_tic, bw_tic, iops_tic) = (self._conv_byte_tic(bs),
                                                  self._conv_byte_tic(bw),
                                                  self._conv_iops_tic(iops))
                    results.append([bs, bs_tic, bw, bw_tic, iops, iops_tic])
                for result in sorted(results):
                    result_str = " ".join( map(lambda x: str(x), result) )
                    print(result_str, file=out)


    def _gen_pdf(self, gp_file):
        subprocess.call("cd %s; gnuplot %s" %
                        (self.out_dir, os.path.basename(gp_file)),
                        shell=True)

    def _get_pdf_name(self):
        pdf_name = self.out_file
        outs = self.out_file.split(".")
        if outs[-1] == "gp" or outs[-1] == "gnuplot":
            pdf_name = '.'.join(outs[0:-1]) + ".pdf"
        pdf_name = os.path.basename(pdf_name)
        return pdf_name

    def _plot_header(self):
        (n_unit, n_col, n_row) = (2, 2, 1)
        print("set term pdfcairo size %sin,%sin font \',10\'" %
              (self.UNIT_WIDTH * n_col, self.UNIT_HEIGHT * n_row),
              file=self.out)
        print("set_out=\'set output \"`if test -z $OUT; then echo %s; else echo $OUT; fi`\"\'"
              % self._get_pdf_name(), file=self.out)
        print("eval set_out", file=self.out)
        print("set multiplot layout %s,%s" % (n_row, n_col), file=self.out)
        print("set xtics nomirror rotate by -45", file=self.out)

    def _plot_footer(self):
        print("", file=self.out)
        print("unset multiplot", file=self.out)
        print("set output", file=self.out)

    def _get_style(self, workload, ncore):
        return "with lp ps 0.5"

    def _plot_data(self, workload, plot_fmt):
        ncores = self._get_ncores(workload)
        ncore  = ncores[0]
        print("plot [0:][0:] \'%s\' using %s title \'ncore=%s\' %s"
              % (self._get_data_file(workload, ncore),
                 plot_fmt,
                 ncore,
                 self._get_style(workload, ncore)),
              end="", file=self.out)
        for ncore in ncores[1:]:
            print(", \'%s\' using %s title \'ncore=%s\' %s"
              % (self._get_data_file(workload, ncore),
                 plot_fmt,
                 ncore,
                 self._get_style(workload, ncore)),
              end="", file=self.out)
        print("", file=self.out)

    def _plot_bw(self, workload):
        print("", file=self.out)
        print("set title \'%s'" % self._get_pretty_workload(workload),
              file=self.out)
        print("set xlabel \'Block size\'", file=self.out)
        print("set ylabel \'%s\'" % "MB/sec", file=self.out)
        self._plot_data(workload, ":($3/(1024*1024)):xtic(2)")

    def _plot_iops(self, workload):
        print("", file=self.out)
        print("set title \'%s'" % self._get_pretty_workload(workload),
              file=self.out)
        print("set xlabel \'Block size\'", file=self.out)
        print("set ylabel \'%s\'" % "IOPS (x1000)", file=self.out)
        self._plot_data(workload, ":($5/1000):xtic(2)")

if __name__ == "__main__":
    optparser = optparse.OptionParser()
    optparser.add_option("--log",      help="log file")
    optparser.add_option("--out",      help="output directory")
    optparser.add_option("--workload", type='choice', choices=['read', 'randread'], help="{read | randread}")
    (opts, args) = optparser.parse_args()

    # check arg: only one of --ncore or --bs should be specified.
    for opt in vars(opts):
        val = getattr(opts, opt)
        if val == None:
            print("Missing options: %s" % opt)
            optparser.print_help()
            exit(1)

    # plotting
    plotter = Plotter(opts.log)
    plotter.plot_bw_iops(opts.workload, opts.out)
