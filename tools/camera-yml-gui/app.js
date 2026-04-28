import * as THREE from "https://unpkg.com/three@0.161.0/build/three.module.js";

const state = {
  cameras: [],
  selectedIndex: -1,
  dragging: false,
  lastPointerX: 0,
  lastPointerY: 0,
  autoSpin: false,
  orbitAzimuth: 0.82,
  orbitElevation: 0.44,
  orbitRadius: 4.8,
};

const ui = {
  viewport: document.getElementById("viewport"),
  addCameraBtn: document.getElementById("addCameraBtn"),
  removeCameraBtn: document.getElementById("removeCameraBtn"),
  cameraSelect: document.getElementById("cameraSelect"),
  camId: document.getElementById("camId"),
  deviceId: document.getElementById("deviceId"),
  summaryTsv: document.getElementById("summaryTsv"),
  posX: document.getElementById("posX"),
  posY: document.getElementById("posY"),
  posZ: document.getElementById("posZ"),
  yawDeg: document.getElementById("yawDeg"),
  pitchDeg: document.getElementById("pitchDeg"),
  rollDeg: document.getElementById("rollDeg"),
  minCameras: document.getElementById("minCameras"),
  weighting: document.getElementById("weighting"),
  generateBtn: document.getElementById("generateBtn"),
  downloadBtn: document.getElementById("downloadBtn"),
  yamlOutput: document.getElementById("yamlOutput"),
};

const scene = new THREE.Scene();
scene.background = new THREE.Color(0x0a1017);

const trackerStyle = {
  normalBase: 0x9aa9bf,
  normalCone: 0xffb050,
  selectedBase: 0x49dcb1,
  selectedCone: 0xff5f7a,
};

const camera = new THREE.PerspectiveCamera(
  55,
  ui.viewport.clientWidth / ui.viewport.clientHeight,
  0.01,
  200
);
camera.position.set(3.0, 2.0, 3.2);
camera.lookAt(0, 0, 0);

function updateOrbitCamera() {
  const ce = Math.cos(state.orbitElevation);
  const se = Math.sin(state.orbitElevation);
  const ca = Math.cos(state.orbitAzimuth);
  const sa = Math.sin(state.orbitAzimuth);

  camera.position.set(
    state.orbitRadius * ce * ca,
    state.orbitRadius * se,
    state.orbitRadius * ce * sa
  );
  camera.lookAt(0, 0, 0);
}

updateOrbitCamera();

const renderer = new THREE.WebGLRenderer({ antialias: true });
renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
renderer.setSize(ui.viewport.clientWidth, ui.viewport.clientHeight);
ui.viewport.appendChild(renderer.domElement);

const ambient = new THREE.HemisphereLight(0x8db9ff, 0x202020, 0.8);
scene.add(ambient);
const dir = new THREE.DirectionalLight(0xffffff, 0.8);
dir.position.set(3, 4, 2);
scene.add(dir);

const grid = new THREE.GridHelper(8, 16, 0x35506a, 0x1c2a3a);
grid.position.y = -1.2;
scene.add(grid);

const axes = new THREE.AxesHelper(1.8);
scene.add(axes);

function createSatelliteBody() {
  const sat = new THREE.Group();

  const bus = new THREE.Mesh(
    new THREE.BoxGeometry(1.8, 1.0, 1.2),
    new THREE.MeshStandardMaterial({
      color: 0x4f6780,
      metalness: 0.25,
      roughness: 0.72,
    })
  );
  sat.add(bus);

  const deck = new THREE.Mesh(
    new THREE.BoxGeometry(1.25, 0.08, 0.9),
    new THREE.MeshStandardMaterial({ color: 0x6f8396, metalness: 0.2, roughness: 0.65 })
  );
  deck.position.set(0, 0.54, 0);
  sat.add(deck);

  const antenna = new THREE.Mesh(
    new THREE.CylinderGeometry(0.08, 0.08, 0.24, 20),
    new THREE.MeshStandardMaterial({ color: 0xaec4d8, metalness: 0.3, roughness: 0.5 })
  );
  antenna.rotation.z = Math.PI / 2;
  antenna.position.set(0.96, 0.25, 0);
  sat.add(antenna);

  const panelMat = new THREE.MeshStandardMaterial({
    color: 0x2f4f79,
    metalness: 0.1,
    roughness: 0.82,
  });

  const panelLeft = new THREE.Mesh(new THREE.BoxGeometry(0.9, 0.03, 2.0), panelMat);
  panelLeft.position.set(0, 0, -1.62);
  sat.add(panelLeft);

  const panelRight = new THREE.Mesh(new THREE.BoxGeometry(0.9, 0.03, 2.0), panelMat);
  panelRight.position.set(0, 0, 1.62);
  sat.add(panelRight);

  const frame = new THREE.LineSegments(
    new THREE.EdgesGeometry(new THREE.BoxGeometry(1.8, 1.0, 1.2)),
    new THREE.LineBasicMaterial({ color: 0xb4c6d6 })
  );
  sat.add(frame);

  const bodyFrameAxes = new THREE.AxesHelper(1.25);
  sat.add(bodyFrameAxes);

  return sat;
}

const satBody = createSatelliteBody();
scene.add(satBody);

function clampNumber(value, fallback = 0) {
  const n = Number(value);
  return Number.isFinite(n) ? n : fallback;
}

function degToRad(v) {
  return (v * Math.PI) / 180.0;
}

function normalize3(vec) {
  const n = Math.hypot(vec[0], vec[1], vec[2]);
  if (n <= 0) {
    return [0, 0, 0];
  }
  return [vec[0] / n, vec[1] / n, vec[2] / n];
}

function round6(v) {
  return Math.round(v * 1e6) / 1e6;
}

function createTrackerMesh() {
  const group = new THREE.Group();

  const baseMaterial = new THREE.MeshStandardMaterial({ color: trackerStyle.normalBase });
  const coneMaterial = new THREE.MeshStandardMaterial({ color: trackerStyle.normalCone });

  const base = new THREE.Mesh(
    new THREE.BoxGeometry(0.12, 0.08, 0.08),
    baseMaterial
  );
  group.add(base);

  const cone = new THREE.Mesh(
    new THREE.ConeGeometry(0.04, 0.12, 20),
    coneMaterial
  );
  cone.rotation.z = -Math.PI / 2;
  cone.position.x = 0.11;
  group.add(cone);

  // Show each tracker's local body-frame orientation clearly.
  const localAxes = new THREE.AxesHelper(0.34);
  group.add(localAxes);

  group.userData.baseMaterial = baseMaterial;
  group.userData.coneMaterial = coneMaterial;

  satBody.add(group);
  return group;
}

function setTrackerSelected(mesh, isSelected) {
  const baseMaterial = mesh.userData.baseMaterial;
  const coneMaterial = mesh.userData.coneMaterial;
  if (!baseMaterial || !coneMaterial) {
    return;
  }

  baseMaterial.color.setHex(isSelected ? trackerStyle.selectedBase : trackerStyle.normalBase);
  coneMaterial.color.setHex(isSelected ? trackerStyle.selectedCone : trackerStyle.normalCone);
}

function refreshTrackerHighlights() {
  state.cameras.forEach((cam, idx) => {
    setTrackerSelected(cam.mesh, idx === state.selectedIndex);
  });
}

function addCameraTracker() {
  const i = state.cameras.length;
  const mesh = createTrackerMesh();

  const cam = {
    id: `cam${i}`,
    device_id: `/dev/video${i}`,
    summary_tsv: `logs/cam${i}_run/solve/summary.tsv`,
    position_body: [0.9, 0.0, 0.0],
    yaw_deg: 0,
    pitch_deg: 0,
    roll_deg: 0,
    mesh,
  };

  state.cameras.push(cam);
  state.selectedIndex = state.cameras.length - 1;
  applyCameraToMesh(cam);
  refreshCameraSelect();
  refreshTrackerHighlights();
  loadSelectedCameraIntoForm();
}

function removeSelectedTracker() {
  if (state.selectedIndex < 0 || state.selectedIndex >= state.cameras.length) {
    return;
  }

  const cam = state.cameras[state.selectedIndex];
  satBody.remove(cam.mesh);
  state.cameras.splice(state.selectedIndex, 1);

  if (state.cameras.length === 0) {
    state.selectedIndex = -1;
  } else {
    state.selectedIndex = Math.max(0, state.selectedIndex - 1);
  }

  refreshCameraSelect();
  refreshTrackerHighlights();
  loadSelectedCameraIntoForm();
}

function refreshCameraSelect() {
  ui.cameraSelect.innerHTML = "";

  state.cameras.forEach((cam, idx) => {
    const opt = document.createElement("option");
    opt.value = String(idx);
    opt.textContent = `${idx}: ${cam.id}`;
    ui.cameraSelect.appendChild(opt);
  });

  if (state.selectedIndex >= 0) {
    ui.cameraSelect.value = String(state.selectedIndex);
  }
}

function applyCameraToMesh(cam) {
  cam.mesh.position.set(cam.position_body[0], cam.position_body[1], cam.position_body[2]);

  const e = new THREE.Euler(
    degToRad(cam.roll_deg),
    degToRad(cam.pitch_deg),
    degToRad(cam.yaw_deg),
    "ZYX"
  );
  cam.mesh.setRotationFromEuler(e);
}

function loadSelectedCameraIntoForm() {
  const cam = state.cameras[state.selectedIndex];
  const enabled = Boolean(cam);

  refreshTrackerHighlights();

  [
    ui.camId,
    ui.deviceId,
    ui.summaryTsv,
    ui.posX,
    ui.posY,
    ui.posZ,
    ui.yawDeg,
    ui.pitchDeg,
    ui.rollDeg,
    ui.removeCameraBtn,
  ].forEach((el) => {
    el.disabled = !enabled;
  });

  if (!cam) {
    ui.camId.value = "";
    ui.deviceId.value = "";
    ui.summaryTsv.value = "";
    ui.posX.value = "";
    ui.posY.value = "";
    ui.posZ.value = "";
    ui.yawDeg.value = "";
    ui.pitchDeg.value = "";
    ui.rollDeg.value = "";
    return;
  }

  ui.camId.value = cam.id;
  ui.deviceId.value = cam.device_id;
  ui.summaryTsv.value = cam.summary_tsv;
  ui.posX.value = String(cam.position_body[0]);
  ui.posY.value = String(cam.position_body[1]);
  ui.posZ.value = String(cam.position_body[2]);
  ui.yawDeg.value = String(cam.yaw_deg);
  ui.pitchDeg.value = String(cam.pitch_deg);
  ui.rollDeg.value = String(cam.roll_deg);
}

function saveFormToSelectedCamera() {
  const cam = state.cameras[state.selectedIndex];
  if (!cam) {
    return;
  }

  cam.id = ui.camId.value.trim() || cam.id;
  cam.device_id = ui.deviceId.value.trim();
  cam.summary_tsv = ui.summaryTsv.value.trim();

  cam.position_body = [
    clampNumber(ui.posX.value),
    clampNumber(ui.posY.value),
    clampNumber(ui.posZ.value),
  ];

  cam.yaw_deg = clampNumber(ui.yawDeg.value);
  cam.pitch_deg = clampNumber(ui.pitchDeg.value);
  cam.roll_deg = clampNumber(ui.rollDeg.value);

  applyCameraToMesh(cam);
  refreshCameraSelect();
}

function cameraVectorsFromEuler(cam) {
  const q = new THREE.Quaternion().setFromEuler(
    new THREE.Euler(
      degToRad(cam.roll_deg),
      degToRad(cam.pitch_deg),
      degToRad(cam.yaw_deg),
      "ZYX"
    )
  );

  const boresight = new THREE.Vector3(1, 0, 0).applyQuaternion(q);
  const up = new THREE.Vector3(0, 1, 0).applyQuaternion(q);

  return {
    boresight_body: normalize3([boresight.x, boresight.y, boresight.z]).map(round6),
    up_body: normalize3([up.x, up.y, up.z]).map(round6),
  };
}

function yamlValueArray(arr) {
  return `[${arr[0]}, ${arr[1]}, ${arr[2]}]`;
}

function buildYaml() {
  const lines = [];
  lines.push("frame:");
  lines.push("  reference: satellite_body");
  lines.push("  note: all position/orientation/vector fields below are in satellite body frame");
  lines.push("");
  lines.push("cameras:");

  state.cameras.forEach((cam) => {
    const vectors = cameraVectorsFromEuler(cam);
    const pos = cam.position_body.map(round6);

    lines.push(`  - id: ${cam.id}`);
    lines.push(`    device_id: ${cam.device_id}`);
    lines.push(`    position_body: ${yamlValueArray(pos)}`);
    lines.push(`    yaw_pitch_roll_deg: [${round6(cam.yaw_deg)}, ${round6(cam.pitch_deg)}, ${round6(cam.roll_deg)}]`);
    lines.push(`    boresight_body: ${yamlValueArray(vectors.boresight_body)}`);
    lines.push(`    up_body: ${yamlValueArray(vectors.up_body)}`);
    lines.push(`    summary_tsv: ${cam.summary_tsv}`);
    lines.push("");
  });

  lines.push("fusion:");
  lines.push(`  min_cameras: ${Math.max(1, clampNumber(ui.minCameras.value, 2))}`);
  lines.push(`  weighting: ${ui.weighting.value}`);

  return lines.join("\n").trim() + "\n";
}

function generateYamlToTextbox() {
  saveFormToSelectedCamera();
  ui.yamlOutput.value = buildYaml();
}

function downloadYaml() {
  generateYamlToTextbox();
  const blob = new Blob([ui.yamlOutput.value], { type: "text/yaml" });
  const a = document.createElement("a");
  a.href = URL.createObjectURL(blob);
  a.download = "camera.yml";
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  URL.revokeObjectURL(a.href);
}

function bindEvents() {
  ui.addCameraBtn.addEventListener("click", () => {
    addCameraTracker();
    generateYamlToTextbox();
  });

  ui.removeCameraBtn.addEventListener("click", () => {
    removeSelectedTracker();
    generateYamlToTextbox();
  });

  ui.cameraSelect.addEventListener("change", () => {
    state.selectedIndex = clampNumber(ui.cameraSelect.value, 0);
    loadSelectedCameraIntoForm();
    generateYamlToTextbox();
  });

  [
    ui.camId,
    ui.deviceId,
    ui.summaryTsv,
    ui.posX,
    ui.posY,
    ui.posZ,
    ui.yawDeg,
    ui.pitchDeg,
    ui.rollDeg,
    ui.minCameras,
    ui.weighting,
  ].forEach((el) => {
    el.addEventListener("input", () => {
      saveFormToSelectedCamera();
      generateYamlToTextbox();
    });
  });

  ui.generateBtn.addEventListener("click", generateYamlToTextbox);
  ui.downloadBtn.addEventListener("click", downloadYaml);

  window.addEventListener("resize", () => {
    const w = ui.viewport.clientWidth;
    const h = ui.viewport.clientHeight;
    camera.aspect = w / h;
    camera.updateProjectionMatrix();
    renderer.setSize(w, h);
  });

  const dragArea = renderer.domElement;
  dragArea.style.cursor = "grab";

  dragArea.addEventListener("pointerdown", (event) => {
    if (event.button !== 0) {
      return;
    }
    state.dragging = true;
    state.lastPointerX = event.clientX;
    state.lastPointerY = event.clientY;
    dragArea.style.cursor = "grabbing";
    dragArea.setPointerCapture(event.pointerId);
  });

  dragArea.addEventListener("pointermove", (event) => {
    if (!state.dragging) {
      return;
    }

    const dx = event.clientX - state.lastPointerX;
    const dy = event.clientY - state.lastPointerY;
    state.lastPointerX = event.clientX;
    state.lastPointerY = event.clientY;

    state.orbitAzimuth += dx * 0.008;
    state.orbitElevation += dy * 0.008;
    state.orbitElevation = Math.max(-Math.PI * 0.45, Math.min(Math.PI * 0.45, state.orbitElevation));
    updateOrbitCamera();
  });

  dragArea.addEventListener("pointerup", (event) => {
    state.dragging = false;
    dragArea.style.cursor = "grab";
    dragArea.releasePointerCapture(event.pointerId);
  });

  dragArea.addEventListener("pointercancel", (event) => {
    state.dragging = false;
    dragArea.style.cursor = "grab";
    dragArea.releasePointerCapture(event.pointerId);
  });

  dragArea.addEventListener(
    "wheel",
    (event) => {
      event.preventDefault();
      const direction = Math.sign(event.deltaY);
      const scale = direction > 0 ? 1.08 : 0.92;
      state.orbitRadius *= scale;
      state.orbitRadius = Math.max(1.2, Math.min(12.0, state.orbitRadius));
      updateOrbitCamera();
    },
    { passive: false }
  );
}

function tick() {
  if (state.autoSpin && !state.dragging) {
    state.orbitAzimuth += 0.0025;
    updateOrbitCamera();
  }
  renderer.render(scene, camera);
  requestAnimationFrame(tick);
}

bindEvents();
addCameraTracker();
addCameraTracker();
generateYamlToTextbox();
tick();
