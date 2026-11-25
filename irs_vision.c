/*
 * irs_vision.c - IRS goruntu isleme kutuphanesi (fonksiyonlarin kodu)
 * Imzalar ve aciklamalar: irs_vision.h
 */

#include "irs_vision.h"

void rgb_to_hsv(color pixel, float *h, float *s, float *v)
{
    float r = pixel.r / 255.0f;
    float g = pixel.g / 255.0f;
    float b = pixel.b / 255.0f;

    float largest = r;
    float smallest = r;

    if (g > largest) largest = g;
    if (b > largest) largest = b;

    if (g < smallest) smallest = g;
    if (b < smallest) smallest = b;

    float difference = largest - smallest;
    float hue;

    if (difference == 0.0f)
        hue = 0.0f;
    else if (largest == r)
        hue = 60.0f * ((g - b) / difference);
    else if (largest == g)
        hue = 60.0f * ((b - r) / difference + 2.0f);
    else
        hue = 60.0f * ((r - g) / difference + 4.0f);

    if (hue < 0.0f)
        hue += 360.0f;

    *h = hue / 2.0f;

    if (largest == 0.0f)
        *s = 0.0f;
    else
        *s = (difference / largest) * 255.0f;

    *v = largest * 255.0f;
}

/* cv2.inRange karsiligi: araliktaki piksellere 255, digerlerine 0 */
unsigned char **hsv_maske(color **img, int NC, int NR, HsvAralik a)
{
    unsigned char **maske = alloc_img(NC, NR);

    for (int y = 0; y < NR; y++)
    {
        for (int x = 0; x < NC; x++)
        {
            float h, s, v;
            rgb_to_hsv(img[y][x], &h, &s, &v);

            if (h >= a.hmin && h <= a.hmax &&
                s >= a.smin && s <= a.smax &&
                v >= a.vmin && v <= a.vmax)
                maske[y][x] = 255;
            else
                maske[y][x] = 0;
        }
    }

    return maske;
}

/* cv2.bitwise_or karsiligi: ekle'deki beyazlari hedef'e de yazar */
void maske_or(unsigned char **hedef, unsigned char **ekle, int NC, int NR)
{
    for (int y = 0; y < NR; y++)
    {
        for (int x = 0; x < NC; x++)
        {
            if (ekle[y][x] == 255)
                hedef[y][x] = 255;
        }
    }
}

unsigned char **erode(unsigned char **src, int NC, int NR, int k)
{
    unsigned char **result = alloc_img(NC, NR);
    int r = k / 2;

    for (int y = 0; y < NR; y++)
    {
        for (int x = 0; x < NC; x++)
        {
            int keep_white = 1;

            for (int dy = -r; dy <= r; dy++)
            {
                for (int dx = -r; dx <= r; dx++)
                {
                    int ny = y + dy;
                    int nx = x + dx;

                    if (ny >= 0 && ny < NR &&
                        nx >= 0 && nx < NC)
                    {
                        if (src[ny][nx] == 0)
                            keep_white = 0;
                    }
                }
            }

            result[y][x] = keep_white ? 255 : 0;
        }
    }

    return result;
}

unsigned char **dilate(unsigned char **src, int NC, int NR, int k)
{
    unsigned char **result = alloc_img(NC, NR);
    int r = k / 2;

    for (int y = 0; y < NR; y++)
    {
        for (int x = 0; x < NC; x++)
        {
            int make_white = 0;

            for (int dy = -r; dy <= r; dy++)
            {
                for (int dx = -r; dx <= r; dx++)
                {
                    int ny = y + dy;
                    int nx = x + dx;

                    if (ny >= 0 && ny < NR &&
                        nx >= 0 && nx < NC)
                    {
                        if (src[ny][nx] == 255)
                            make_white = 1;
                    }
                }
            }

            result[y][x] = make_white ? 255 : 0;
        }
    }

    return result;
}

/* Morfolojik acma: once erode (kucuk benekleri sil), sonra dilate (kalan sekilleri eski boyuna getir) */
unsigned char **acma(unsigned char **src, int NC, int NR, int k)
{
    unsigned char **asinmis = erode(src, NC, NR, k);
    unsigned char **sonuc   = dilate(asinmis, NC, NR, k);

    free_img(asinmis);   /* ara sonuc, artik lazim degil */
    return sonuc;
}


/* Kalin (2 piksel) cizgi - Bresenham */
void draw_line(color **img, int NC, int NR,
               int x0, int y0, int x1, int y1, color c)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (1)
    {
        for (int ty = 0; ty <= 1; ty++)
            for (int tx = 0; tx <= 1; tx++)
            {
                int px = x0 + tx, py = y0 + ty;
                if (px >= 0 && px < NC && py >= 0 && py < NR)
                    img[py][px] = c;
            }

        if (x0 == x1 && y0 == y1)
            break;

        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/*
 * cv2.minAreaRect yerine: parcanin piksellerinden ana yonu (PCA / moment)
 * bulunur, pikseller bu yone ve dik yone izdusurulur -> dondurulmus kutu.
 * Isik cubugu gibi uzun-ince sekillerde minAreaRect ile neredeyse ayni sonucu verir.
 * pixels[i] = y * NC + x
 */
void bolge_hesapla(const int *pixels, int count, int NC, Bolge *b)
{
    double mx = 0, my = 0;

    for (int i = 0; i < count; i++)
    {
        mx += pixels[i] % NC;
        my += pixels[i] / NC;
    }
    mx /= count;
    my /= count;

    double sxx = 0, syy = 0, sxy = 0;

    for (int i = 0; i < count; i++)
    {
        double dx = pixels[i] % NC - mx;
        double dy = pixels[i] / NC - my;
        sxx += dx * dx;
        syy += dy * dy;
        sxy += dx * dy;
    }

    double angle = 0.5 * atan2(2.0 * sxy, sxx - syy);
    double ux = cos(angle), uy = sin(angle);   /* ana eksen */
    double vx = -uy,        vy = ux;           /* dik eksen */

    double amin = 1e9, amax = -1e9, bmin = 1e9, bmax = -1e9;

    for (int i = 0; i < count; i++)
    {
        double dx = pixels[i] % NC - mx;
        double dy = pixels[i] / NC - my;
        double a = dx * ux + dy * uy;
        double b = dx * vx + dy * vy;

        if (a < amin) amin = a;
        if (a > amax) amax = a;
        if (b < bmin) bmin = b;
        if (b > bmax) bmax = b;
    }

    double w = amax - amin;   /* ana eksen boyunca uzunluk */
    double h = bmax - bmin;   /* dik eksen boyunca uzunluk */

    double a_s[4] = { amin, amax, amax, amin };
    double b_s[4] = { bmin, bmin, bmax, bmax };

    for (int i = 0; i < 4; i++)
    {
        b->kose[i][0] = mx + a_s[i] * ux + b_s[i] * vx;
        b->kose[i][1] = my + a_s[i] * uy + b_s[i] * vy;
    }

    b->cx     = mx;
    b->cy     = my;
    b->uzun   = w > h ? w : h;
    b->kisa   = w > h ? h : w;
    b->aci    = (w >= h ? angle : angle + 1.5707963) * 180.0 / 3.14159265;
    b->piksel = count;
    b->parlaklik = 0;          /* sonradan parlaklik_hesapla doldurur */
    b->renk      = RENK_YOK;   /* sonradan renk_hesapla doldurur */
}

/* Bolge filtreye uyuyor mu? (Colab: aspect_ratio < 0.25 and 50 < max_side < 110) */
int sekle_uyar_mi(const Bolge *b, SekilFiltre f)
{
    if (b->uzun == 0 || b->kisa == 0)
        return 0;

    return (b->kisa / b->uzun) < f.max_oran &&
           b->uzun > f.min_uzun && b->uzun < f.max_uzun &&
           b->parlaklik >= f.min_parlaklik &&
           b->piksel >= f.min_piksel &&
           (f.max_aci_sapma <= 0 || aci_farki(b->aci, f.hedef_aci) <= f.max_aci_sapma);
}

/* Alan filtresi: min_piksel'den kucuk parcalari diziden siler */
void kucuk_bolgeleri_sil(Bolge *b, int *n, int min_piksel)
{
    int yaz = 0;                      /* bir sonraki "kalan" parcanin yazilacagi yer */

    for (int oku = 0; oku < *n; oku++)
    {
        if (b[oku].piksel >= min_piksel)
        {
            b[yaz] = b[oku];          /* yeterince buyuk: one dogru tasi */
            yaz++;
        }
        /* kucukse hicbir sey yapma: uzerine yazilacak */
    }

    *n = yaz;                         /* yeni parca sayisi */
}

/*
 * Dondurulmus kutunun icindeki piksellerin ortalama parlakligi (HSV'deki V = max(r,g,b)).
 * Isik kendisi parlar (V yuksek), metal kasa sadece yansitir (V dusuk).
 * Kutular cizilmeden ONCE cagrilmali, yoksa yesil cizgiler sonucu bozar.
 */
void parlaklik_hesapla(color **img, int NC, int NR, Bolge *b)
{
    double rad = b->aci * 3.14159265 / 180.0;
    double ux = cos(rad), uy = sin(rad);
    double toplam = 0;
    int adet = 0;

    for (int y = b->y; y < b->y + b->boy && y < NR; y++)
    {
        for (int x = b->x; x < b->x + b->en && x < NC; x++)
        {
            /* piksel dondurulmus kutunun icinde mi? */
            double dx = x - b->cx, dy = y - b->cy;
            double a = dx * ux + dy * uy;
            double c = -dx * uy + dy * ux;

            if (fabs(a) > b->uzun / 2 || fabs(c) > b->kisa / 2)
                continue;

            color p = img[y][x];
            int v = p.r;
            if (p.g > v) v = p.g;
            if (p.b > v) v = p.b;

            toplam += v;
            adet++;
        }
    }

    b->parlaklik = adet > 0 ? toplam / adet : 0;
}

/* Bolgenin dondurulmus kutusunu ciz (cv2.drawContours(img, [box], ...)) */
void kutu_ciz(color **img, int NC, int NR, const Bolge *b, color c)
{
    for (int i = 0; i < 4; i++)
    {
        int j = (i + 1) % 4;
        draw_line(img, NC, NR,
                  (int)lround(b->kose[i][0]), (int)lround(b->kose[i][1]),
                  (int)lround(b->kose[j][0]), (int)lround(b->kose[j][1]),
                  c);
    }
}

/* Iki aci arasindaki fark (derece, 0..90). 179 ile -1 derece ayni yondur. */
double aci_farki(double a1, double a2)
{
    double d = fmod(fabs(a1 - a2), 180.0);
    return d > 90.0 ? 180.0 - d : d;
}

/* Iki bolgeyi tek bolgede birlestir: 8 koseyi ortak eksene izdusur, yeni kutu cikar */
Bolge bolge_birlestir(const Bolge *b1, const Bolge *b2)
{
    Bolge m;

    /* ortak yon: uzunluga gore agirlikli ortalama aci (uzun parca daha cok soz sahibi) */
    double a2 = b2->aci;
    if (a2 - b1->aci > 90)  a2 -= 180;    /* 179 ile -1 gibi durumlari ayni tarafa getir */
    if (a2 - b1->aci < -90) a2 += 180;
    double toplam_uzun = b1->uzun + b2->uzun;
    double aci = toplam_uzun > 0
               ? (b1->aci * b1->uzun + a2 * b2->uzun) / toplam_uzun
               : (b1->aci + a2) / 2;   /* iki parca da tek piksel: 0/0 = NaN olmasin */
    double rad = aci * 3.14159265 / 180.0;

    double ux = cos(rad), uy = sin(rad);
    double vx = -uy,      vy = ux;

    double amin = 1e9, amax = -1e9, bmin = 1e9, bmax = -1e9;
    const Bolge *ikisi[2] = { b1, b2 };

    for (int k = 0; k < 2; k++)
        for (int i = 0; i < 4; i++)
        {
            double px = ikisi[k]->kose[i][0], py = ikisi[k]->kose[i][1];
            double a = px * ux + py * uy;
            double b = px * vx + py * vy;
            if (a < amin) amin = a;
            if (a > amax) amax = a;
            if (b < bmin) bmin = b;
            if (b > bmax) bmax = b;
        }

    double a_s[4] = { amin, amax, amax, amin };
    double b_s[4] = { bmin, bmin, bmax, bmax };

    for (int i = 0; i < 4; i++)
    {
        m.kose[i][0] = a_s[i] * ux + b_s[i] * vx;
        m.kose[i][1] = a_s[i] * uy + b_s[i] * vy;
    }

    double ac = (amin + amax) / 2, bc = (bmin + bmax) / 2;
    m.cx   = ac * ux + bc * vx;
    m.cy   = ac * uy + bc * vy;
    m.uzun = amax - amin;
    m.kisa = bmax - bmin;
    m.aci  = aci;
    m.piksel = b1->piksel + b2->piksel;
    m.parlaklik = 0;
    m.renk      = RENK_YOK;

    /* eksene hizali kutu: ikisini kapsayan kutu */
    int sag1 = b1->x + b1->en, sag2 = b2->x + b2->en;
    int alt1 = b1->y + b1->boy, alt2 = b2->y + b2->boy;
    m.x   = b1->x < b2->x ? b1->x : b2->x;
    m.y   = b1->y < b2->y ? b1->y : b2->y;
    m.en  = (sag1 > sag2 ? sag1 : sag2) - m.x;
    m.boy = (alt1 > alt2 ? alt1 : alt2) - m.y;

    return m;
}

/*
 * Ayni dogru uzerinde, ayni acida ve birbirine yakin parcalari birlestirir
 * (orn. ortasinda karanlik bosluk olan isik cubugu iki parca cikarsa).
 *   max_aci   : aci farki bundan kucukse (derece)
 *   max_bosluk: uclar arasi bosluk bundan kucukse (piksel)
 *   max_kayma : uzun eksene dik yondeki kayma bundan kucukse (piksel)
 * Dizi yerinde guncellenir, *n azalir.
 */
void bolgeleri_birlestir(Bolge *b, int *n,
                         double max_aci, double max_bosluk, double max_kayma)
{
    int degisti = 1;

    while (degisti)
    {
        degisti = 0;

        for (int i = 0; i < *n && !degisti; i++)
        {
            for (int j = i + 1; j < *n && !degisti; j++)
            {
                if (aci_farki(b[i].aci, b[j].aci) > max_aci)
                    continue;

                /* j'nin merkezini i'nin eksenine gore olc */
                double rad = b[i].aci * 3.14159265 / 180.0;
                double dx = b[j].cx - b[i].cx;
                double dy = b[j].cy - b[i].cy;
                double boyunca = fabs(dx * cos(rad) + dy * sin(rad));
                double dik     = fabs(-dx * sin(rad) + dy * cos(rad));

                double bosluk = boyunca - (b[i].uzun + b[j].uzun) / 2;

                if (bosluk > max_bosluk || dik > max_kayma)
                    continue;

                b[i] = bolge_birlestir(&b[i], &b[j]);

                /* j'yi diziden cikar: sonrakileri bir sola kaydir */
                for (int k = j; k < *n - 1; k++)
                    b[k] = b[k + 1];
                (*n)--;

                degisti = 1;   /* bastan tekrar bak: yeni parca baskalariyla da birlesebilir */
            }
        }
    }
}

/*
 * cv2.findContours + cv2.minAreaRect karsiligi.
 * Maskedeki beyaz parcalari bulur, her biri icin Bolge doldurur.
 * *liste yeni ayrilan diziyi gosterir; isin bitince free(*liste) yap.
 * Donus: bulunan parca sayisi.
 */
int bolgeleri_bul(unsigned char **src, int NC, int NR, Bolge **liste)
{
    size_t count = (size_t)NC * NR;

    unsigned char *visited = calloc(count, sizeof(*visited));
    int *queue = malloc(count * sizeof(*queue));
    int region = 0;
    int kapasite = 16;
    Bolge *bolgeler = malloc(kapasite * sizeof(Bolge));

    *liste = NULL;

    if (visited == NULL || queue == NULL || bolgeler == NULL)
    {
        free(visited);
        free(queue);
        free(bolgeler);
        printf("Bellek ayrilamadi.\n");
        return 0;
    }

    for (int y = 0; y < NR; y++)
    {
        for (int x = 0; x < NC; x++)
        {
            int start = y * NC + x;

            if (src[y][x] == 0 || visited[start])
                continue;

            int head = 0, tail = 0;
            int left = x, right = x;
            int top = y, bottom = y;

            queue[tail++] = start;
            visited[start] = 1;

            while (head < tail)
            {
                int pos = queue[head++];
                int cy = pos / NC;
                int cx = pos % NC;

                if (cx < left) left = cx;
                if (cx > right) right = cx;
                if (cy < top) top = cy;
                if (cy > bottom) bottom = cy;

                for (int dy = -1; dy <= 1; dy++)
                {
                    for (int dx = -1; dx <= 1; dx++)
                    {
                        int ny = cy + dy;
                        int nx = cx + dx;

                        if (ny < 0 || ny >= NR ||
                            nx < 0 || nx >= NC)
                            continue;

                        int next = ny * NC + nx;

                        if (src[ny][nx] == 255 && !visited[next])
                        {
                            visited[next] = 1;
                            queue[tail++] = next;
                        }
                    }
                }
            }

            /* dizi doldu mu? doluysa iki katina buyut */
            if (region == kapasite)
            {
                kapasite *= 2;
                Bolge *yeni = realloc(bolgeler, kapasite * sizeof(Bolge));
                if (yeni == NULL)
                {
                    printf("Bellek ayrilamadi.\n");
                    break;
                }
                bolgeler = yeni;
            }

            /* queue[0..tail-1] = bu parcanin tum pikselleri */
            Bolge *b = &bolgeler[region];
            bolge_hesapla(queue, tail, NC, b);
            b->x   = left;
            b->y   = top;
            b->en  = right - left + 1;
            b->boy = bottom - top + 1;

            region++;
        }
    }

    free(queue);
    free(visited);

    *liste = bolgeler;
    return region;
}


/* ================================================================== */
/*  Zirh tespiti                                                        */
/* ================================================================== */

/*
 * Isigin rengi: kutunun biraz genisletilmis halindeki mavi ve kirmizi toplamini karsilastirir.
 * Isigin ortasi doymus beyazdir (R=G=B=255), rengi asil etraftaki hale (glow) belli eder.
 * O yuzden kutuyu her yone "pay" kadar genisletiyoruz.
 */
void renk_hesapla(color **img, int NC, int NR, Bolge *b)
{
    int pay = (int)b->kisa + 3;
    long toplam_mavi = 0, toplam_kirmizi = 0;

    for (int y = b->y - pay; y < b->y + b->boy + pay; y++)
    {
        if (y < 0 || y >= NR) continue;

        for (int x = b->x - pay; x < b->x + b->en + pay; x++)
        {
            if (x < 0 || x >= NC) continue;

            toplam_mavi    += img[y][x].b;
            toplam_kirmizi += img[y][x].r;
        }
    }

    b->renk = toplam_mavi > toplam_kirmizi ? RENK_MAVI : RENK_KIRMIZI;
}

/*
 * Isigin iki ucu: dondurulmus kutunun kisa kenarlarinin orta noktalari.
 * bolge_hesapla koseleri su sirayla verir: 0=(amin,bmin) 1=(amax,bmin) 2=(amax,bmax) 3=(amin,bmax)
 * Uzun eksen "a" oldugunda uclar 0-3 ve 1-2 kenarlarinin ortasidir.
 * Kisa eksen "a" ise (nadir) uclar 0-1 ve 2-3 kenarlarinin ortasidir.
 */
void isik_uclari(const Bolge *b, double ust[2], double alt[2])
{
    double k01 = hypot(b->kose[1][0] - b->kose[0][0], b->kose[1][1] - b->kose[0][1]);
    double k03 = hypot(b->kose[3][0] - b->kose[0][0], b->kose[3][1] - b->kose[0][1]);
    double u1[2], u2[2];

    if (k01 >= k03)   /* 0-1 kenari uzun kenar -> uclar 0-3 ve 1-2 kenarlarinda */
    {
        u1[0] = (b->kose[0][0] + b->kose[3][0]) / 2;  u1[1] = (b->kose[0][1] + b->kose[3][1]) / 2;
        u2[0] = (b->kose[1][0] + b->kose[2][0]) / 2;  u2[1] = (b->kose[1][1] + b->kose[2][1]) / 2;
    }
    else
    {
        u1[0] = (b->kose[0][0] + b->kose[1][0]) / 2;  u1[1] = (b->kose[0][1] + b->kose[1][1]) / 2;
        u2[0] = (b->kose[2][0] + b->kose[3][0]) / 2;  u2[1] = (b->kose[2][1] + b->kose[3][1]) / 2;
    }

    /* ust = ekranda yukaridaki uc = y'si kucuk olan */
    const double *ilk = u1[1] < u2[1] ? u1 : u2;
    const double *son = u1[1] < u2[1] ? u2 : u1;
    ust[0] = ilk[0];  ust[1] = ilk[1];
    alt[0] = son[0];  alt[1] = son[1];
}

/* zirh_bul'un ic kullanimi icin: bir aday isik cifti ve "ne kadar kotu" puani */
typedef struct {
    int i, j;
    double maliyet;
} ZirhAdayi;

/* static = sadece bu dosyanin icinden kullanilir, .h'de yok (ic yardimci fonksiyon) */
static void zirh_doldur(const Bolge *a, const Bolge *b, double mesafe_orani, Zirh *z)
{
    double a_ust[2], a_alt[2], b_ust[2], b_alt[2];
    isik_uclari(a, a_ust, a_alt);
    isik_uclari(b, b_ust, b_alt);

    /* hangisi solda? uclarin ortalama x'ine bak */
    int a_solda = (a_ust[0] + a_alt[0]) < (b_ust[0] + b_alt[0]);
    const Bolge *sol = a_solda ? a : b;
    const Bolge *sag = a_solda ? b : a;
    double *sol_ust = a_solda ? a_ust : b_ust, *sol_alt = a_solda ? a_alt : b_alt;
    double *sag_ust = a_solda ? b_ust : a_ust, *sag_alt = a_solda ? b_alt : a_alt;

    z->sol = *sol;
    z->sag = *sag;

    z->kose[0][0] = sol_ust[0];  z->kose[0][1] = sol_ust[1];
    z->kose[1][0] = sag_ust[0];  z->kose[1][1] = sag_ust[1];
    z->kose[2][0] = sag_alt[0];  z->kose[2][1] = sag_alt[1];
    z->kose[3][0] = sol_alt[0];  z->kose[3][1] = sol_alt[1];

    /* merkez = dort ucun ortalamasi */
    z->cx = (z->kose[0][0] + z->kose[1][0] + z->kose[2][0] + z->kose[3][0]) / 4;
    z->cy = (z->kose[0][1] + z->kose[1][1] + z->kose[2][1] + z->kose[3][1]) / 4;

    z->mesafe_orani = mesafe_orani;
    z->renk = sol->renk;
}

int zirh_bul(const Bolge *isiklar, int n, ZirhFiltre f, Zirh **liste)
{
    *liste = NULL;
    if (n < 2)
        return 0;

    /* 1) Tum ikilileri dene, kurallara uyanlari aday listesine yaz */
    ZirhAdayi *aday = malloc((size_t)n * n * sizeof(ZirhAdayi));
    int aday_sayisi = 0;

    if (aday == NULL)
        return 0;

    for (int i = 0; i < n; i++)
    {
        for (int j = i + 1; j < n; j++)
        {
            const Bolge *a = &isiklar[i];
            const Bolge *b = &isiklar[j];

            /* Kural 1: ayni renk */
            if (a->renk == RENK_YOK || a->renk != b->renk)
                continue;

            /* Kural 2: paralel */
            double fark = aci_farki(a->aci, b->aci);
            if (fark > f.max_aci_farki)
                continue;

            /* Kural 3: benzer boy */
            double kisa_boy = a->uzun < b->uzun ? a->uzun : b->uzun;
            double uzun_boy = a->uzun < b->uzun ? b->uzun : a->uzun;
            double boy_orani = kisa_boy / uzun_boy;
            if (boy_orani < f.min_boy_orani)
                continue;

            /* Kural 4 ve 5: merkezler arasi vektoru isik yonune gore olc */
            double ort_boy = (a->uzun + b->uzun) / 2;
            double rad = a->aci * 3.14159265 / 180.0;
            double dx = b->cx - a->cx;
            double dy = b->cy - a->cy;

            double kayma  = fabs( dx * cos(rad) + dy * sin(rad)) / ort_boy;  /* isik yonunde */
            double mesafe = fabs(-dx * sin(rad) + dy * cos(rad)) / ort_boy;  /* isige dik */

            if (kayma > f.max_kayma)                                  /* ayni hizada degil */
                continue;
            if (mesafe < f.min_mesafe || mesafe > f.max_mesafe)       /* cok yakin / cok uzak */
                continue;

            /* Maliyet: her kuraldan ne kadar "zorlandigi" (kucuk = iyi eslesme) */
            aday[aday_sayisi].i = i;
            aday[aday_sayisi].j = j;
            aday[aday_sayisi].maliyet = fark / f.max_aci_farki
                                      + (1.0 - boy_orani)
                                      + kayma;
            aday_sayisi++;
        }
    }

    /* 2) Adaylari maliyete gore sirala (kucukten buyuge) - basit secmeli siralama */
    for (int a = 0; a < aday_sayisi - 1; a++)
    {
        int en_iyi = a;
        for (int b = a + 1; b < aday_sayisi; b++)
            if (aday[b].maliyet < aday[en_iyi].maliyet)
                en_iyi = b;

        ZirhAdayi gecici = aday[a];
        aday[a] = aday[en_iyi];
        aday[en_iyi] = gecici;
    }

    /* 3) En iyiden baslayarak sec; bir isik ikinci kez kullanilmasin */
    unsigned char *kullanildi = calloc(n, 1);
    Zirh *zirhlar = malloc((size_t)(n / 2 + 1) * sizeof(Zirh));
    int zirh_sayisi = 0;

    if (kullanildi == NULL || zirhlar == NULL)
    {
        free(aday);
        free(kullanildi);
        free(zirhlar);
        return 0;
    }

    for (int k = 0; k < aday_sayisi; k++)
    {
        int i = aday[k].i, j = aday[k].j;
        if (kullanildi[i] || kullanildi[j])
            continue;

        kullanildi[i] = kullanildi[j] = 1;

        const Bolge *a = &isiklar[i];
        const Bolge *b = &isiklar[j];
        double ort_boy = (a->uzun + b->uzun) / 2;
        double rad = a->aci * 3.14159265 / 180.0;
        double mesafe = fabs(-(b->cx - a->cx) * sin(rad) + (b->cy - a->cy) * cos(rad)) / ort_boy;

        zirh_doldur(a, b, mesafe, &zirhlar[zirh_sayisi]);
        zirh_sayisi++;
    }

    free(aday);
    free(kullanildi);

    *liste = zirhlar;
    return zirh_sayisi;
}

void zirh_ciz(color **img, int NC, int NR, const Zirh *z, color c)
{
    /* dort kenar */
    for (int i = 0; i < 4; i++)
    {
        int j = (i + 1) % 4;
        draw_line(img, NC, NR,
                  (int)lround(z->kose[i][0]), (int)lround(z->kose[i][1]),
                  (int)lround(z->kose[j][0]), (int)lround(z->kose[j][1]),
                  c);
    }

    /* merkezde arti isareti */
    int mx = (int)lround(z->cx), my = (int)lround(z->cy);
    draw_line(img, NC, NR, mx - 8, my, mx + 8, my, c);
    draw_line(img, NC, NR, mx, my - 8, mx, my + 8, c);
}
