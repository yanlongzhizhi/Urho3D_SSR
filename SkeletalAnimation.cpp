//
// Copyright (c) 2008-2017 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Engine/Engine.h>
#include <Urho3D/Graphics/AnimatedModel.h>
#include <Urho3D/Graphics/Animation.h>
#include <Urho3D/Graphics/AnimationState.h>
#include <Urho3D/Graphics/Camera.h>
#include <Urho3D/Graphics/DebugRenderer.h>
#include <Urho3D/Graphics/Graphics.h>
#include <Urho3D/Graphics/Light.h>
#include <Urho3D/Graphics/Material.h>
#include <Urho3D/Graphics/Octree.h>
#include <Urho3D/Graphics/Renderer.h>
#include <Urho3D/Graphics/Zone.h>
#include <Urho3D/Input/Input.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/Scene/Scene.h>
#include <Urho3D/UI/Font.h>
#include <Urho3D/UI/Text.h>
#include <Urho3D/UI/Button.h>
#include <Urho3D/UI/UI.h>
#include <Urho3D/UI/UIEvents.h>
#include <Urho3D/Graphics/GraphicsDefs.h>

#include "Mover.h"
#include "SkeletalAnimation.h"

#include <Urho3D/DebugNew.h>

URHO3D_DEFINE_APPLICATION_MAIN(SkeletalAnimation)

SkeletalAnimation::SkeletalAnimation(Context* context) :
    Sample(context),
    drawDebug_(false)
{
    // Register an object factory for our custom Mover component so that we can create them to scene nodes
    context->RegisterFactory<Mover>();
}

void SkeletalAnimation::Start()
{
    // Execute base class startup
    Sample::Start();

    // Create the scene content
    CreateScene();

    // Create the UI content
    //CreateInstructions();

    // Setup the viewport for displaying the scene
    SetupViewport();

    // Hook up to the frame update and render post-update events
    SubscribeToEvents();

    // Set the mouse mode to use in the sample
    Sample::InitMouseMode(MM_ABSOLUTE);
}

void SkeletalAnimation::CreateScene()
{
    ResourceCache* cache = GetSubsystem<ResourceCache>();

    scene_ = new Scene(context_);

    // Create octree, use default volume (-1000, -1000, -1000) to (1000, 1000, 1000)
    // Also create a DebugRenderer component so that we can draw debug geometry
    scene_->CreateComponent<Octree>();
    scene_->CreateComponent<DebugRenderer>();

    // Create scene node & StaticModel component for showing a static plane
    Node* planeNode = scene_->CreateChild("Plane");
    planeNode->SetScale(Vector3(50.0f, 1.0f, 50.0f));
    StaticModel* planeObject = planeNode->CreateComponent<StaticModel>();
    planeObject->SetModel(cache->GetResource<Model>("Models/Plane.mdl"));
    planeObject->SetMaterial(cache->GetResource<Material>("Materials/NoTextureWithSSR.xml"));

    // Create a Zone component for ambient lighting & fog control
    Node* zoneNode = scene_->CreateChild("Zone");
    Zone* zone = zoneNode->CreateComponent<Zone>();
    zone->SetBoundingBox(BoundingBox(-1000.0f, 1000.0f));
    zone->SetAmbientColor(Color(0.5f, 0.5f, 0.5f));
    zone->SetFogColor(Color(0.4f, 0.5f, 0.8f));
    zone->SetFogStart(100.0f);
    zone->SetFogEnd(300.0f);

    // Create a directional light to the world. Enable cascaded shadows on it
    Node* lightNode = scene_->CreateChild("DirectionalLight");
    lightNode->SetDirection(Vector3(0.6f, -1.0f, 0.8f));
    Light* light = lightNode->CreateComponent<Light>();
    light->SetLightType(LIGHT_DIRECTIONAL);
    light->SetCastShadows(true);
    light->SetColor(Color(0.5f, 0.5f, 0.5f));
    light->SetShadowBias(BiasParameters(0.00025f, 0.5f));
    // Set cascade splits at 10, 50 and 200 world units, fade shadows out at 80% of maximum shadow distance
    light->SetShadowCascade(CascadeParameters(10.0f, 50.0f, 200.0f, 0.0f, 0.8f));

    // Create animated models
    const unsigned NUM_MODELS = 30;
    const float MODEL_MOVE_SPEED = 2.0f;
    const float MODEL_ROTATE_SPEED = 100.0f;
    const BoundingBox bounds(Vector3(-20.0f, 0.0f, -20.0f), Vector3(20.0f, 0.0f, 20.0f));

    for (unsigned i = 0; i < NUM_MODELS; ++i)
    {
        Node* modelNode = scene_->CreateChild("Jill");
        modelNode->SetPosition(Vector3(Random(40.0f) - 20.0f, 0.0f, Random(40.0f) - 20.0f));
        modelNode->SetRotation(Quaternion(0.0f, Random(360.0f), 0.0f));

        AnimatedModel* modelObject = modelNode->CreateComponent<AnimatedModel>();
        modelObject->SetModel(cache->GetResource<Model>("Models/Kachujin/Kachujin.mdl"));
        modelObject->SetMaterial(cache->GetResource<Material>("Models/Kachujin/Materials/Kachujin.xml"));
        modelObject->SetCastShadows(true);

        // Create an AnimationState for a walk animation. Its time position will need to be manually updated to advance the
        // animation, The alternative would be to use an AnimationController component which updates the animation automatically,
        // but we need to update the model's position manually in any case
        Animation* walkAnimation = cache->GetResource<Animation>("Models/Kachujin/Kachujin_Walk.ani");

        AnimationState* state = modelObject->AddAnimationState(walkAnimation);
        // The state would fail to create (return null) if the animation was not found
        if (state)
        {
            // Enable full blending weight and looping
            state->SetWeight(1.0f);
            state->SetLooped(true);
            state->SetTime(Random(walkAnimation->GetLength()));
        }

        // Create our custom Mover component that will move & animate the model during each frame's update
        Mover* mover = modelNode->CreateComponent<Mover>();
        mover->SetParameters(MODEL_MOVE_SPEED, MODEL_ROTATE_SPEED, bounds);
    }

    // Create the camera. Limit far clip distance to match the fog
    cameraNode_ = scene_->CreateChild("Camera");
    Camera* camera = cameraNode_->CreateComponent<Camera>();
    //camera->SetFarClip(300.0f);
    camera->SetFarClip(50.0f);

    // Set an initial position for the camera scene node above the plane
    cameraNode_->SetPosition(Vector3(0.0f, 5.0f, 0.0f));
    
    
    
    
    
    //liuzhichao-test
    UI* ui = GetSubsystem<UI>();
    XMLFile* style = cache->GetResource<XMLFile>("UI/DefaultStyle.xml");
    ui->GetRoot()->SetDefaultStyle(style);
    
    Graphics* graphics = GetSubsystem<Graphics>();
    
    SampleRadiusLabel_ = ui->GetRoot()->CreateChild<Text>();
    SampleRadiusLabel_->SetFont(cache->GetResource<Font>("Fonts/Anonymous Pro.ttf"), 15);
    SampleRadiusLabel_->SetPosition(370, 50);
    SampleRadiusLabel_->SetTextEffect(TE_SHADOW);
    SampleRadiusLabel_->SetText("SampleRadiu " + Variant(1.0).GetString());
    graphics->SetShaderParameter(PSP_SAMPLERDIUS, Variant(1.0));
    
    BetaLabel_ = ui->GetRoot()->CreateChild<Text>();
    BetaLabel_->SetFont(cache->GetResource<Font>("Fonts/Anonymous Pro.ttf"), 15);
    BetaLabel_->SetPosition(370, 100);
    BetaLabel_->SetTextEffect(TE_SHADOW);
    BetaLabel_->SetText("Beta " + Variant(0.005).GetString());
    graphics->SetShaderParameter(PSP_BETA, Variant(0.005));
    
    EpsLabel_ = ui->GetRoot()->CreateChild<Text>();
    EpsLabel_->SetFont(cache->GetResource<Font>("Fonts/Anonymous Pro.ttf"), 15);
    EpsLabel_->SetPosition(370, 150);
    EpsLabel_->SetTextEffect(TE_SHADOW);
    EpsLabel_->SetText("Eps " + Variant(0.003).GetString());
    graphics->SetShaderParameter(PSP_EPS, Variant(0.003));
    
    SigmaLabel_ = ui->GetRoot()->CreateChild<Text>();
    SigmaLabel_->SetFont(cache->GetResource<Font>("Fonts/Anonymous Pro.ttf"), 15);
    SigmaLabel_->SetPosition(370, 200);
    SigmaLabel_->SetTextEffect(TE_SHADOW);
    SigmaLabel_->SetText("Sigma " + Variant(0.09).GetString());
    graphics->SetShaderParameter(PSP_SIGMA, Variant(0.09));
    
    SampleRadiusIncButton_ = ui->GetRoot()->CreateChild<Button>();
    SampleRadiusIncButton_->SetStyleAuto();
    SampleRadiusIncButton_->SetFixedWidth(30);
    SampleRadiusIncButton_->SetPosition(50, 50);
    SubscribeToEvent(SampleRadiusIncButton_, E_PRESSED, URHO3D_HANDLER(SkeletalAnimation, HandleSampleRadiusIncPressed));
    
    SampleRadiusDecButton_ = ui->GetRoot()->CreateChild<Button>();
    SampleRadiusDecButton_->SetStyleAuto();
    SampleRadiusDecButton_->SetFixedWidth(30);
    SampleRadiusDecButton_->SetPosition(100, 50);
    SubscribeToEvent(SampleRadiusDecButton_, E_PRESSED, URHO3D_HANDLER(SkeletalAnimation, HandleSampleRadiusDecPressed));
    
    BetaIncButton_ = ui->GetRoot()->CreateChild<Button>();
    BetaIncButton_->SetStyleAuto();
    BetaIncButton_->SetFixedWidth(30);
    BetaIncButton_->SetPosition(50, 100);
    SubscribeToEvent(BetaIncButton_, E_PRESSED, URHO3D_HANDLER(SkeletalAnimation, HandleBetaIncPressed));
    
    BetaDecButton_ = ui->GetRoot()->CreateChild<Button>();
    BetaDecButton_->SetStyleAuto();
    BetaDecButton_->SetFixedWidth(30);
    BetaDecButton_->SetPosition(100, 100);
    SubscribeToEvent(BetaDecButton_, E_PRESSED, URHO3D_HANDLER(SkeletalAnimation, HandleBetaDecPressed));
    
    EpsIncButton_ = ui->GetRoot()->CreateChild<Button>();
    EpsIncButton_->SetStyleAuto();
    EpsIncButton_->SetFixedWidth(30);
    EpsIncButton_->SetPosition(50, 150);
    SubscribeToEvent(EpsIncButton_, E_PRESSED, URHO3D_HANDLER(SkeletalAnimation, HandleEpsIncPressed));
    
    EpsDecButton_ = ui->GetRoot()->CreateChild<Button>();
    EpsDecButton_->SetStyleAuto();
    EpsDecButton_->SetFixedWidth(30);
    EpsDecButton_->SetPosition(100, 150);
    SubscribeToEvent(EpsDecButton_, E_PRESSED, URHO3D_HANDLER(SkeletalAnimation, HandleEpsDecPressed));
    
    SigmaIncButton_ = ui->GetRoot()->CreateChild<Button>();
    SigmaIncButton_->SetStyleAuto();
    SigmaIncButton_->SetFixedWidth(30);
    SigmaIncButton_->SetPosition(50, 200);
    SubscribeToEvent(SigmaIncButton_, E_PRESSED, URHO3D_HANDLER(SkeletalAnimation, HandleSigmaIncPressed));
    
    SigmaDecButton_ = ui->GetRoot()->CreateChild<Button>();
    SigmaDecButton_->SetStyleAuto();
    SigmaDecButton_->SetFixedWidth(30);
    SigmaDecButton_->SetPosition(100, 200);
    SubscribeToEvent(SigmaDecButton_, E_PRESSED, URHO3D_HANDLER(SkeletalAnimation, HandleSigmaDecPressed));
    
    Node* mushroomNode = scene_->CreateChild("Mushroom");
    mushroomNode->SetPosition(Vector3(0.0f, 0.0f, 0.0f));
    mushroomNode->SetScale(2.0f);
    StaticModel* mushroomObject = mushroomNode->CreateComponent<StaticModel>();
    mushroomObject->SetModel(cache->GetResource<Model>("Models/Mushroom.mdl"));
    mushroomObject->SetMaterial(cache->GetResource<Material>("Materials/Mushroom.xml"));
}

void SkeletalAnimation::HandleSampleRadiusIncPressed(StringHash eventType, VariantMap& eventData)
{
    Graphics* graphics = GetSubsystem<Graphics>();
    float SampleRadiusValue = Variant(SampleRadiusLabel_->GetText()).GetFloat();
    SampleRadiusValue += 0.01;
    graphics->SetShaderParameter(PSP_SAMPLERDIUS, SampleRadiusValue);
    SampleRadiusLabel_->SetText("SampleRadius: " + Variant(SampleRadiusValue).GetString());
}

void SkeletalAnimation::HandleSampleRadiusDecPressed(StringHash eventType, VariantMap& eventData)
{
    Graphics* graphics = GetSubsystem<Graphics>();
    float SampleRadiusValue = Variant(SampleRadiusLabel_->GetText()).GetFloat();
    SampleRadiusValue -= 0.01;
    graphics->SetShaderParameter(PSP_SAMPLERDIUS, SampleRadiusValue);
    SampleRadiusLabel_->SetText("SampleRadius: " + Variant(SampleRadiusValue).GetString());
}

void SkeletalAnimation::HandleBetaIncPressed(StringHash eventType, VariantMap& eventData)
{
    Graphics* graphics = GetSubsystem<Graphics>();
    float BetaValue = Variant(BetaLabel_->GetText()).GetFloat();
    BetaValue += 0.01;
    graphics->SetShaderParameter(PSP_BETA, BetaValue);
    BetaLabel_->SetText("Beta: " + Variant(BetaValue).GetString());
}

void SkeletalAnimation::HandleBetaDecPressed(StringHash eventType, VariantMap& eventData)
{
    Graphics* graphics = GetSubsystem<Graphics>();
    float BetaValue = Variant(BetaLabel_->GetText()).GetFloat();
    BetaValue -= 0.01;
    graphics->SetShaderParameter(PSP_BETA, BetaValue);
    BetaLabel_->SetText("Beta: " + Variant(BetaValue).GetString());
}

void SkeletalAnimation::HandleEpsIncPressed(StringHash eventType, VariantMap& eventData)
{
    Graphics* graphics = GetSubsystem<Graphics>();
    float EpsValue = Variant(EpsLabel_->GetText()).GetFloat();
    EpsValue += 0.01;
    graphics->SetShaderParameter(PSP_EPS, EpsValue);
    EpsLabel_->SetText("Eps: " + Variant(EpsValue).GetString());
}

void SkeletalAnimation::HandleEpsDecPressed(StringHash eventType, VariantMap& eventData)
{
    Graphics* graphics = GetSubsystem<Graphics>();
    float EpsValue = Variant(EpsLabel_->GetText()).GetFloat();
    EpsValue -= 0.01;
    graphics->SetShaderParameter(PSP_EPS, EpsValue);
    EpsLabel_->SetText("Eps: " + Variant(EpsValue).GetString());
}

void SkeletalAnimation::HandleSigmaIncPressed(StringHash eventType, VariantMap& eventData)
{
    Graphics* graphics = GetSubsystem<Graphics>();
    float SigmaValue = Variant(SigmaLabel_->GetText()).GetFloat();
    SigmaValue += 0.01;
    graphics->SetShaderParameter(PSP_SIGMA, SigmaValue);
    SigmaLabel_->SetText("Sigma: " + Variant(SigmaValue).GetString());
}

void SkeletalAnimation::HandleSigmaDecPressed(StringHash eventType, VariantMap& eventData)
{
    Graphics* graphics = GetSubsystem<Graphics>();
    float SigmaValue = Variant(SigmaLabel_->GetText()).GetFloat();
    SigmaValue -= 0.01;
    graphics->SetShaderParameter(PSP_SIGMA, SigmaValue);
    SigmaLabel_->SetText("Sigma: " + Variant(SigmaValue).GetString());
}

void SkeletalAnimation::CreateInstructions()
{
    ResourceCache* cache = GetSubsystem<ResourceCache>();
    UI* ui = GetSubsystem<UI>();

    // Construct new Text object, set string to display and font to use
    Text* instructionText = ui->GetRoot()->CreateChild<Text>();
    instructionText->SetText(
        "Use WASD keys and mouse/touch to move\n"
        "Space to toggle debug geometry"
    );
    instructionText->SetFont(cache->GetResource<Font>("Fonts/Anonymous Pro.ttf"), 15);
    // The text has multiple rows. Center them in relation to each other
    instructionText->SetTextAlignment(HA_CENTER);

    // Position the text relative to the screen center
    instructionText->SetHorizontalAlignment(HA_CENTER);
    instructionText->SetVerticalAlignment(VA_CENTER);
    instructionText->SetPosition(0, ui->GetRoot()->GetHeight() / 4);
}

void SkeletalAnimation::SetupViewport()
{
    Renderer* renderer = GetSubsystem<Renderer>();

    // Set up a viewport to the Renderer subsystem so that the 3D scene can be seen
    SharedPtr<Viewport> viewport(new Viewport(context_, scene_, cameraNode_->GetComponent<Camera>()));
    renderer->SetViewport(0, viewport);
}

void SkeletalAnimation::SubscribeToEvents()
{
    // Subscribe HandleUpdate() function for processing update events
    SubscribeToEvent(E_UPDATE, URHO3D_HANDLER(SkeletalAnimation, HandleUpdate));

    // Subscribe HandlePostRenderUpdate() function for processing the post-render update event, sent after Renderer subsystem is
    // done with defining the draw calls for the viewports (but before actually executing them.) We will request debug geometry
    // rendering during that event
    SubscribeToEvent(E_POSTRENDERUPDATE, URHO3D_HANDLER(SkeletalAnimation, HandlePostRenderUpdate));
}

void SkeletalAnimation::MoveCamera(float timeStep)
{
    // Do not move if the UI has a focused element (the console)
    if (GetSubsystem<UI>()->GetFocusElement())
        return;

    Input* input = GetSubsystem<Input>();

    // Movement speed as world units per second
    const float MOVE_SPEED = 20.0f;
    // Mouse sensitivity as degrees per pixel
    const float MOUSE_SENSITIVITY = 0.1f;

    // Use this frame's mouse motion to adjust camera node yaw and pitch. Clamp the pitch between -90 and 90 degrees
    IntVector2 mouseMove = input->GetMouseMove();
    yaw_ += MOUSE_SENSITIVITY * mouseMove.x_;
    pitch_ += MOUSE_SENSITIVITY * mouseMove.y_;
    pitch_ = Clamp(pitch_, -90.0f, 90.0f);

    // Construct new orientation for the camera scene node from yaw and pitch. Roll is fixed to zero
    cameraNode_->SetRotation(Quaternion(pitch_, yaw_, 0.0f));

    // Read WASD keys and move the camera scene node to the corresponding direction if they are pressed
    if (input->GetKeyDown(KEY_W))
        cameraNode_->Translate(Vector3::FORWARD * MOVE_SPEED * timeStep);
    if (input->GetKeyDown(KEY_S))
        cameraNode_->Translate(Vector3::BACK * MOVE_SPEED * timeStep);
    if (input->GetKeyDown(KEY_A))
        cameraNode_->Translate(Vector3::LEFT * MOVE_SPEED * timeStep);
    if (input->GetKeyDown(KEY_D))
        cameraNode_->Translate(Vector3::RIGHT * MOVE_SPEED * timeStep);

    // Toggle debug geometry with space
    if (input->GetKeyPress(KEY_SPACE))
        drawDebug_ = !drawDebug_;
}

void SkeletalAnimation::HandleUpdate(StringHash eventType, VariantMap& eventData)
{
    using namespace Update;

    // Take the frame time step, which is stored as a float
    float timeStep = eventData[P_TIMESTEP].GetFloat();

    // Move the camera, scale movement with time step
    MoveCamera(timeStep);
}

void SkeletalAnimation::HandlePostRenderUpdate(StringHash eventType, VariantMap& eventData)
{
    // If draw debug mode is enabled, draw viewport debug geometry, which will show eg. drawable bounding boxes and skeleton
    // bones. Note that debug geometry has to be separately requested each frame. Disable depth test so that we can see the
    // bones properly
    if (drawDebug_)
        GetSubsystem<Renderer>()->DrawDebugGeometry(false);
}
