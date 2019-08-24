"""
This program generates the Burning Man map that will be displayed in the receiver.

Requires Python 3 (tested with 3.7.4)

The GPS coordinates and other BRC layout info come from http://innovate.burningman.org/datasets-page/
(And more specifically, for 2019: https://bm-innovate.s3.amazonaws.com/2019/2019_BRC_Measurements.pdf)

Usage:
pip install Pillow
python gen_map.py
"""

from PIL import Image, ImageDraw
from math import *

# GPS coordinates of the man
man_coords = (40.78598, -119.20584)

# GPS coordinates of each fence point
fence_point_coords = [
  (40.78236, -119.23530),  # bottom left
  (40.80570, -119.21965),  # top left
  (40.80163, -119.18533),  # top point (directly above the Man)
  (40.77568, -119.17971),  # top right
  (40.76373, -119.21050),  # bottom right
]

TOP_FENCE_PT = 2  # index in fence_point_coords of the top fence point

# Size of the image in pixels
img_width = 129  # Using an odd width makes everything easier, especially for drawing circles
img_height = 200

# Coordinates of the center pixel in the image
center_px = (int(img_width / 2), int(img_height / 2))

# Average distance in feet between the Man and each fence point.
# Use avg_fence_point_distance() below to calculate
fence_point_dist = 8155

# Radius of the Earth in feet at the latitude of the Man, and 3,904 feet elevation - https://rechneronline.de/earth-radius/
earth_radius = 6369056 * 3.2808  # feet

# Distance between the Man and the center of each road
# Source: https://bm-innovate.s3.amazonaws.com/2019/2019_BRC_Measurements.pdf
# Note: we have the center of L at 5,680' whereas the doc says "The outer road is 11,370’ in diameter". Assuming they
# measure from the center of the road, this puts L at 5,685 instead. Not sure where the 5' difference comes from.
roads = [
  2500,  # Center of the Esplanade road
  2940,  # Center of A road
  3230,  # Center of B road
  3520,  # Center of C road
  3810,  # Center of D road
  4100,  # Center of E road
  4340,  # Center of F road
  4580,  # Center of G road
  4820,  # Center of H road
  5060,  # Center of I road
  5300,  # Center of J road
  5490,  # Center of K road
  5680,  # Center of L road
]

# Indices of some roads in the list above
ESP = 0
L = 12

# Distance in feet from the Man to the center of Center Camp
center_camp_dist = 3026

# Radius of Center Camp in feet (radius to the center of Rod's road)
center_camp_radius = 783

# Scale we want to use for the map
# We use the diameter of the outer road so that the city fills the screen horizontally
feet_per_pixel = 2 * roads[L] / img_width


def ft(x):
  """ Converts a distance in feet into an integer number of pixels """
  return round(x / feet_per_pixel)


def h2deg(hour):
  """Converts hours to angles in degrees"""
  return (hour - 3) / 12 * 360


def centered_box(radius, center=center_px):
  return (
    int(center[0] - radius),
    int(center[1] - radius),
    int(center[0] + radius),
    int(center[1] + radius))


def polar2px(a, d):
  """Converts polar coordinates to a pixel location
  a: angle in degrees
  d: distance from the Man in feet
  """
  d = ft(d)
  return (
    round(d * cos(radians(a))) + center_px[0],
    round(d * sin(radians(a))) + center_px[1],
  )


def draw_map():
  im = Image.new('1', (img_width - 1, img_height), 'black')
  draw = ImageDraw.Draw(im)

  # Annulars: draw every other road
  for road in [roads[i] for i in range(ESP, L + 1, 2)]:
    draw.point(circle_points(center_px, ft(road), h2deg(2), h2deg(10)), fill='white')

  # Radials: draw every half hour
  for h in range(2 * 2, 2 * 10 + 1):
    draw.line([polar2px(h2deg(h / 2), roads[ESP]), polar2px(h2deg(h / 2), roads[L])], 'white', 1)

  # Center camp
  cc_pos = ((center_px[0]), (center_px[1] + ft(center_camp_dist)))
  # Draw a black to erase the background, then draw the circle
  draw.ellipse(centered_box(ft(center_camp_radius), cc_pos), outline=None, fill='black')
  draw.point(circle_points(cc_pos, ft(center_camp_radius)), fill='white')

  # Man
  draw.point(circle_points(center_px, 1), fill='white')

  # Temple
  draw.point(circle_points(polar2px(h2deg(12), roads[ESP]), 1), fill='white')

  # Draw the roads at 12, 3, 6 and 9 o'clock from the Man
  dots = []
  for i in range(0, ft(roads[ESP]) - 2, 4):
    dots.extend([
      (center_px[0] + i, center_px[1]),
      (center_px[0] - i, center_px[1]),
      (center_px[0], center_px[1] + i),
      (center_px[0], center_px[1] - i),
    ])
  draw.point(dots, fill='white')

  # Pentagon
  for i in range(0, 5):
    draw.line(
      [polar2px(i * 360 / 5 - 18, fence_point_dist),
       polar2px((i + 1) * 360 / 5 - 18, fence_point_dist)],
      'white', 1)

  del draw
  im.save('map.png')
  output_code(im)
  # im.show()


def circle_points(center, radius, start=0, end=0):
  """
  The function to draw circles in the PIL library is pretty bad, the circles have ugly artifacts.
  So here's another one, stolen from http://degenerateconic.com/midpoint-circle-algorithm/
  :param center: pixel coordinates of the center of the circle
  :param radius: radius of the circle in pixels
  :param start: starting angle in degrees, to draw an arc
  :param end: end angle in degrees, to draw an arc
  :return: list of pixel coordinates of the circle
  """
  result = []
  x0 = center[0]
  y0 = center[1]
  x = radius
  y = 0
  err = 1 - x

  def append(x, y):
    if start == end:  # full circle
      result.append((x, y))
    else:  # arc, check if we should add this point
      angle = degrees(atan2(y - y0, x - x0))
      if (angle - start) % 360 <= (end - start) % 360:
        result.append((x, y))

  while x >= y:
    append(x + x0, y + y0)
    append(y + x0, x + y0)
    append(-x + x0, y + y0)
    append(-y + x0, x + y0)
    append(-x + x0, -y + y0)
    append(-y + x0, -x + y0)
    append(x + x0, -y + y0)
    append(y + x0, -x + y0)

    y = y + 1

    if err < 0:
      err = err + 2 * y + 1
    else:
      x = x - 1
      err = err + 2 * (y - x + 1)

  return result


def gps_dist(p1, p2):
  """Returns the distance and angle between 2 GPS coordinates.

  Source: https://www.movable-type.co.uk/scripts/latlong.html
  Uses the equirectangular approximation, which for an area the size of Burning Man, is accurate down to the millimeter
  when compared to the haversine formula.

  The returned distance will be in whatever units earth_radius is.
  """
  lat1 = radians(p1[0])
  lat2 = radians(p2[0])
  delta_lon = radians(p2[1] - p1[1])

  x = delta_lon * cos((lat1 + lat2) / 2)
  y = lat2 - lat1
  return (sqrt(x * x + y * y) * earth_radius,  # distance between the 2 points
          atan2(y, x))  # angle in radians between the vector p1_p2 and the x axis

def avg_fence_point_distance():
  return round(sum([gps_dist(man_coords, p)[0] for p in fence_point_coords]) / 5)

def output_code(img):
  """
  Converts the image to cpp code
  This is equivalent to http://javl.github.io/image2cpp/
  """

  # This works because we create the image with Image.new('1', ...)
  # The '1' tells PIL to store the image with 1 bit per pixel, which is the format we want
  bytes = img.tobytes(encoder_name='raw')

  bytes_per_line = int(img.width / 8)
  lines = []
  last_non_empty = -1
  skipped_beginning = 0 # how many lines we skipped from the beginning
  for i in range(0, len(bytes), bytes_per_line):
    # Get one image line (128 pixels) worth of bytes
    line = bytes[i:i+bytes_per_line]

    if any(line):  # if this is not an empty line
        last_non_empty = len(lines)
    elif last_non_empty == -1:
      # Skip empty lines at the beginning
      skipped_beginning += 1
      continue

    # Convert to hex
    lines.append("".join("0x{:02x}, ".format(x) for x in line))


  trimmed_lines = lines[0:last_non_empty+1]
  code = ""
  code = code + "#define MAP_ANGLE      (HALF_PI - {})\n".format(gps_dist(man_coords, fence_point_coords[TOP_FENCE_PT])[1])
  code = code + "#define FEET_PER_PIXEL {}\n".format(feet_per_pixel)
  code = code + "#define MAN_X          {}  // Coordinates of the Man in the map, in pixels\n".format(center_px[0])
  code = code + "#define MAN_Y          {}\n".format(center_px[1] - skipped_beginning)
  code = code + "#define MAN_LAT        {}  // Man coordinates in millionth of degrees\n".format(int(man_coords[0]*1e6))
  code = code + "#define MAN_LON      {}  // {} degrees\n".format(int(man_coords[1]*1e6), man_coords[1])
  code = code + "\n"
  code = code + "// Burning Man map, {0}x{1}px\n".format(img.width, len(trimmed_lines))
  code = code + "const unsigned char bm_map[] PROGMEM = {\n    "
  code = code + "\n    ".join(trimmed_lines)
  code = code[:-2] + "\n};"
  print(code)



if __name__ == '__main__':
  draw_map()
