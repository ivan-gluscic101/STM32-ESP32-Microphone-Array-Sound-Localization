#include "web_server.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "WEB_SERVER";

static httpd_handle_t server = NULL;

/* Lista WebSocket file descriptora spojenih klijenata */
#define MAX_WS_CLIENTS 4
static int ws_fds[MAX_WS_CLIENTS];
static int ws_fd_count = 0;

/* ------------------------------------------------------------------ */
/*  HTML stranica s Three.js vizualizacijom                            */
/* ------------------------------------------------------------------ */
static const char *INDEX_HTML =
"<!DOCTYPE html>"
"<html lang='hr'>"
"<head>"
"<meta charset='UTF-8'>"
"<title>Sound Localization</title>"
"<style>"
"  body { margin: 0; background: #111; }"
"  #info { position:absolute; top:10px; left:10px; color:#fff; font-family:monospace; font-size:14px; }"
"</style>"
"</head>"
"<body>"
"<div id='info'>Angle: --&deg; &nbsp; Strength: --</div>"
"<script src='https://cdn.jsdelivr.net/npm/three@0.160.0/build/three.min.js'></script>"
"<script>"
"  const scene = new THREE.Scene();"
"  scene.background = new THREE.Color(0x111111);"

"  const camera = new THREE.PerspectiveCamera(60, window.innerWidth/window.innerHeight, 0.1, 100);"
"  camera.position.set(0, 8, 8);"
"  camera.lookAt(0, 0, 0);"

"  const renderer = new THREE.WebGLRenderer({ antialias: true });"
"  renderer.setSize(window.innerWidth, window.innerHeight);"
"  document.body.appendChild(renderer.domElement);"

"  window.addEventListener('resize', () => {"
"    camera.aspect = window.innerWidth / window.innerHeight;"
"    camera.updateProjectionMatrix();"
"    renderer.setSize(window.innerWidth, window.innerHeight);"
"  });"

/* Plane (pod) */
"  const planeGeo = new THREE.PlaneGeometry(10, 10);"
"  const planeMat = new THREE.MeshBasicMaterial({ color: 0x222222, side: THREE.DoubleSide });"
"  const plane = new THREE.Mesh(planeGeo, planeMat);"
"  plane.rotation.x = Math.PI / 2;"
"  scene.add(plane);"

/* Grid helper */
"  const grid = new THREE.GridHelper(10, 10, 0x444444, 0x333333);"
"  scene.add(grid);"

/* Mikrokontroler - kocka u centru */
"  const mcuGeo = new THREE.BoxGeometry(0.5, 0.5, 0.5);"
"  const mcuMat = new THREE.MeshBasicMaterial({ color: 0x00aaff });"
"  const mcu = new THREE.Mesh(mcuGeo, mcuMat);"
"  mcu.position.set(0, 0.25, 0);"
"  scene.add(mcu);"

/* Sfera koja prikazuje smjer zvuka */
"  const sphereGeo = new THREE.SphereGeometry(0.2, 16, 16);"
"  const sphereMat = new THREE.MeshBasicMaterial({ color: 0xff4400 });"
"  const sphere = new THREE.Mesh(sphereGeo, sphereMat);"
"  sphere.visible = false;"
"  scene.add(sphere);"

/* Crta od centra do sfere */
"  const lineMat = new THREE.LineBasicMaterial({ color: 0xff4400 });"
"  const lineGeo = new THREE.BufferGeometry().setFromPoints([new THREE.Vector3(0,0.25,0), new THREE.Vector3(0,0.25,0)]);"
"  const line = new THREE.Line(lineGeo, lineMat);"
"  scene.add(line);"

"  const RADIUS = 3.5;"
"  const infoDiv = document.getElementById('info');"

"  function updatePosition(angleDeg, strength) {"
"    const rad = (angleDeg * Math.PI) / 180.0;"
"    const x = RADIUS * Math.sin(rad);"
"    const z = RADIUS * Math.cos(rad);"
"    sphere.position.set(x, 0.2, z);"
"    sphere.visible = true;"
"    const pts = [new THREE.Vector3(0,0.25,0), new THREE.Vector3(x,0.2,z)];"
"    line.geometry.setFromPoints(pts);"
"    line.geometry.attributes.position.needsUpdate = true;"
"    const alpha = Math.min(strength / 100.0, 1.0);"
"    sphereMat.color.setRGB(1.0, 0.27 * (1-alpha), 0);"
"    infoDiv.innerHTML = 'Angle: ' + angleDeg.toFixed(1) + '&deg; &nbsp; Strength: ' + strength;"
"  }"

/* WebSocket */
"  const ws = new WebSocket('ws://' + location.host + '/ws');"
"  ws.onmessage = (event) => {"
"    try {"
"      const d = JSON.parse(event.data);"
"      updatePosition(d.angle, d.strength);"
"    } catch(e) {}"
"  };"
"  ws.onclose = () => { infoDiv.innerHTML += ' [WS disconnected]'; };"

"  function animate() {"
"    requestAnimationFrame(animate);"
"    renderer.render(scene, camera);"
"  }"
"  animate();"
"</script>"
"</body>"
"</html>";

/* ------------------------------------------------------------------ */
/*  HTTP GET / — servira HTML                                          */
/* ------------------------------------------------------------------ */
static esp_err_t root_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/*  WebSocket handler                                                   */
/* ------------------------------------------------------------------ */
static esp_err_t ws_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        /* Nova WebSocket konekcija — spremi fd */
        int fd = httpd_req_to_sockfd(req);
        if (ws_fd_count < MAX_WS_CLIENTS) {
            ws_fds[ws_fd_count++] = fd;
            ESP_LOGI(TAG, "WS klijent spojen, fd=%d", fd);
        }
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(ws_pkt));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) return ret;

    if (ws_pkt.len) {
        uint8_t *buf = calloc(1, ws_pkt.len + 1);
        ws_pkt.payload = buf;
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        free(buf);
    }
    return ret;
}

/* ------------------------------------------------------------------ */
/*  Slanje podataka svim WS klijentima                                 */
/* ------------------------------------------------------------------ */
void web_server_send_data(float angle, uint8_t strength) {
    if (!server || ws_fd_count == 0) return;

    char json[64];
    snprintf(json, sizeof(json), "{\"angle\":%.1f,\"strength\":%u}", angle, strength);

    httpd_ws_frame_t ws_pkt = {
        .final = true,
        .fragmented = false,
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)json,
        .len = strlen(json),
    };

    for (int i = 0; i < ws_fd_count; i++) {
        esp_err_t err = httpd_ws_send_frame_async(server, ws_fds[i], &ws_pkt);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Greška pri slanju na fd=%d, uklanjam klijenta", ws_fds[i]);
            /* Ukloni ovaj fd iz liste */
            ws_fds[i] = ws_fds[--ws_fd_count];
            i--;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Inicijalizacija servera                                             */
/* ------------------------------------------------------------------ */
void web_server_init(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;

    static const httpd_uri_t root_uri = {
        .uri      = "/",
        .method   = HTTP_GET,
        .handler  = root_get_handler,
        .user_ctx = NULL,
    };

    static const httpd_uri_t ws_uri = {
        .uri          = "/ws",
        .method       = HTTP_GET,
        .handler      = ws_handler,
        .user_ctx     = NULL,
        .is_websocket = true,
    };

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &root_uri);
        httpd_register_uri_handler(server, &ws_uri);
        ESP_LOGI(TAG, "HTTP server pokrenut na portu 80");
    } else {
        ESP_LOGE(TAG, "Greška pri pokretanju HTTP servera");
    }
}
