# latexmk config for this repo.
# The paper lives in paper/main.tex but loads figures and data with paths that
# are relative to THIS folder (the repo root), for example figures/fig_*.tex
# and the CSV tables. So latexmk must always run from the repo root and only
# the output is sent into paper/.

$pdf_mode = 1;                 # build a PDF with pdflatex
$pdflatex = 'pdflatex -interaction=nonstopmode -synctex=1 %O %S';
$out_dir  = 'paper';           # put main.pdf, main.aux, logs here
$aux_dir  = 'paper';
$clean_ext = 'synctex.gz acn acr alg glo gls glg ist fdb_latexmk fls';

# The main file to build when latexmk is called with no argument.
@default_files = ('paper/main.tex');
