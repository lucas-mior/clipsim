#include <stdio.h>
#include <leptonica/allheaders.h>
#include <tesseract/capi.h>

void ocr_image(char **text, char *image_path) {
    printf("ocr_image: %p, %s\n", text, image_path);
    return;
	/* TessBaseAPI *handle; */
	/* PIX *img; */
	/* char *text; */

	/* if((img = pixRead(argv[1])) == NULL) { */
	/* 	fprintf("Error reading image\n"); */
    /* } */

	/* handle = TessBaseAPICreate(); */
	/* if(TessBaseAPIInit3(handle, NULL, "eng") != 0) */
	/* 	die("Error initializing tesseract\n"); */

	/* TessBaseAPISetImage2(handle, img); */
	/* if(TessBaseAPIRecognize(handle, NULL) != 0) */
	/* 	die("Error in Tesseract recognition\n"); */

	/* if((text = TessBaseAPIGetUTF8Text(handle)) == NULL) */
	/* 	die("Error getting text\n"); */

	/* fputs(text, stdout); */

	/* TessDeleteText(text); */
	/* TessBaseAPIEnd(handle); */
	/* TessBaseAPIDelete(handle); */
	/* pixDestroy(&img); */

	/* return; */
}
