./bin/lost continous \
  --focal-length 49 \
  --pixel-size 22.2 \
  --centroid-algo cog \
  --centroid-mag-filter 5 \
  --database conf/my-database.dat \
  --star-id-algo py \
  --angular-tolerance 0.05 \
  --false-stars 1000 \
  --max-mismatch-prob 0.0001 \
  --attitude-algo dqm


# ./bin/lost pipeline \
#   --mode file \
#   --file img_7660.png \
#   --focal-length 49 \
#   --pixel-size 22.2 \
#   --centroid-algo cog \
#   --centroid-mag-filter 5 \
#   --database conf/my-database.dat \
#   --star-id-algo py \
#   --angular-tolerance 0.05 \
#   --false-stars 1000 \
#   --max-mismatch-prob 0.0001 \
#   --attitude-algo dqm \
#   --print-attitude attitude.txt \
#   --plot-output annotated-7660.png


./bin/lost database --max-stars 5000 --kvector --kvector-min-distance 0.2 --kvector-max-distance 15 --kvector-distance-bins 10000 --output conf/my-database.dat

# Blind solve (unknown focal length, unknown pointing).
# Requires astrometry.net (`solve-field` and optional `wcsinfo`) and index FITS files.
# ./bin/lost blind-solve \
#   --image sample/image.png \
#   --index-dir conf/astrometry-index \
#   --output-dir logs/blind-solve \
#   --scale-low 0.1 \
#   --scale-high 120 \
#   --downsample 2 \
#   --timeout 120 \
#   --overwrite