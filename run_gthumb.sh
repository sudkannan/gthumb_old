cd /home/amps/gthumb-2.10.11


rm -rf /tmp/ramdisk/*

sudo umount /tmp/ramdisk/
sudo ~/nvmalloc/nvkernel_test_code/ramdisk_create.sh 4
cp -r /home/amps/sda2/home/amps/summer13/datasets/IMAGE_DATASET/image /tmp/ramdisk/

~/nvmalloc/test/load_file /home/amps/sda2/home/amps/summer13/datasets/IMAGE_DATASET/image 1 1

echo "sleeping for 10... drop buffer cache and sync...." 


/home/amps/gthumb-2.10.11/src/gthumb
