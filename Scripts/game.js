var engine = new BABYLON.NativeEngine();
var scene = new BABYLON.Scene(engine);

engine.runRenderLoop(function () {
    scene.render();
});

// Create an arc-rotate camera with default behavior
scene.createDefaultCamera(true, true, true);
var camera = scene.activeCamera;
camera.setTarget(BABYLON.Vector3.Zero());
camera.position = new BABYLON.Vector3(0, 5, -10);

// Hemispheric light aiming up
var light = new BABYLON.HemisphericLight("light", new BABYLON.Vector3(0, 1, 0), scene);
light.intensity = 0.7;

// Sphere
var sphere = BABYLON.MeshBuilder.CreateSphere("sphere", { diameter: 2, segments: 32 }, scene);
sphere.position.y = 1;

// Ground
var ground = BABYLON.MeshBuilder.CreateGround("ground", { width: 6, height: 6 }, scene);
