#!python3
import argparse
import os
from datetime import datetime, timezone, timedelta, tzinfo
from PIL import Image
import piexif
from GPSPhoto import gpsphoto

# exif_fix.py . "2011:07:31 11:11:11" "22.577832,88.4007126"
OUT_DIR="out"

class IST(tzinfo):
    def utcoffset(self, dt):
        return timedelta(hours=5, minutes=30)

    def tzname(self, dt):
        return "IST"

    def dst(self, dt):
        return timedelta(0)

def update_exif(args, filename):
    filetime = datetime.strptime(args.DATE_TAKEN, "%Y:%m:%d %H:%M:%S")
    filetime.replace(tzinfo=IST())
    filetime+= timedelta(minutes=update_exif.counter*args.time)
    update_exif.counter+=1

    # set exif data
    time_stamp = "{}".format(filetime.strftime("%Y:%m:%d %H:%M:%S"))

    exif_ifd = {piexif.ExifIFD.DateTimeOriginal: u"{}".format(filetime.strftime("%Y:%m:%d %H:%M:%S")) }
    gps_ifd = {piexif.GPSIFD.GPSVersionID: (2, 0, 0, 0),
           piexif.GPSIFD.GPSAltitudeRef: 1,
           piexif.GPSIFD.GPSDateStamp: u"{}".format(filetime.strftime("%Y:%m:%d %H:%M:%S")),
           }
    exif_dict = {  "Exif":exif_ifd, "GPS":gps_ifd }
    exif_bytes = piexif.dump(exif_dict)

    file_out = r"{}/{}.jpg".format(OUT_DIR,filetime.strftime("%Y_%m_%d_%H_%M_%S"))
    
    im = Image.open(filename)
    im.save(file_out,
        exif=exif_bytes)

    photo = gpsphoto.GPSPhoto(file_out)

    # Create GPSInfo Data Object
    coords =  [x.strip() for x in args.LOCATION.split(",")]
    coords = [float(x) for x in coords]

    info = gpsphoto.GPSInfo(coords, alt=83, timeStamp=time_stamp)

    # Modify GPS Data
    photo.modGPSData(info, file_out)

update_exif.counter = 0

def is_image(file_path):
    return file_path.lower().endswith(('.jpg','.jpeg','.png'))

def main():
    parser = argparse.ArgumentParser()

    parser.add_argument("DIRECTORY", help="directory with files or one filename")
    parser.add_argument("DATE_TAKEN", help="date photos were taken")
    parser.add_argument("LOCATION", help="GPS coordinates")
    parser.add_argument("--time", type=int, default=1, help="space photos by minutes")

    args = parser.parse_args()

    
    try:
        os.mkdir(OUT_DIR)
    except  FileExistsError:
        pass

    if is_image(args.DIRECTORY):
        return update_exif(args, args.DIRECTORY)

    # Directory specified, process all images
    files = os.listdir(args.DIRECTORY)
    #filter to images only
    files = [x for x in files if is_image(x) ]

    for filename in files:
        update_exif(args, filename)

    

if __name__ == "__main__":
    main()