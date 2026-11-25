/* Zirh tespiti icin kesif: farkli maskelerde dikey isiklar nasil gorunuyor? */
#include <stdio.h>
#include "../irs_vision.h"

static void dene(color **img, int NC, int NR, const char *ad, HsvAralik a, int k)
{
    unsigned char **m = hsv_maske(img, NC, NR, a);
    unsigned char **t = acma(m, NC, NR, k);
    char dosya[64];
    sprintf(dosya, "deneyler/zirh_%s.pgm", ad);
    img_to_pgm_file(t, dosya, NC, NR);

    Bolge *b;
    int n = bolgeleri_bul(t, NC, NR, &b);
    kucuk_bolgeleri_sil(b, &n, 15);
    printf("\n=== %s (kernel %d): %d parca ===\n", ad, k, n);
    for (int i = 0; i < n; i++)
        if (b[i].y > 450 && b[i].y < 750 && b[i].x > 700 && b[i].x < 1300)
            printf("x=%4d y=%4d uzun=%5.1f kisa=%5.1f oran=%.2f aci=%6.1f piksel=%4d\n",
                   b[i].x, b[i].y, b[i].uzun, b[i].kisa, b[i].kisa / b[i].uzun, b[i].aci, b[i].piksel);
    free(b);
    free_img(t);
    free_img(m);
}

int main(void)
{
    int NC, NR;
    color **img = ppm_file_to_img("arena.ppm", &NC, &NR);

    dene(img, NC, NR, "beyaz",    (HsvAralik){0, 0, 240, 180, 30, 255}, 1);
    dene(img, NC, NR, "parlak230", (HsvAralik){0, 0, 230, 180, 255, 255}, 1);
    dene(img, NC, NR, "parlak200", (HsvAralik){0, 0, 200, 180, 255, 255}, 3);

    free_color_img(img);
    return 0;
}
