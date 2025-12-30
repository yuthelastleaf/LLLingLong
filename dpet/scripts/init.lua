-- Desktop Pet Init Script
-- This script is loaded when the pet starts

print("=== Desktop Pet Lua Script Initialized ===")

-- Called when pet starts
function onInit()
    pet.log("Pet is starting up!")
    pet.say("你好！我是桌面宠物~")
end

-- Called when user clicks on the pet
function onClick(x, y)
    pet.log("Clicked at: " .. x .. ", " .. y)
    
    -- Random greetings
    local greetings = {
        "你好呀！",
        "需要什么帮助吗？",
        "喵~",
        "点我干嘛？",
        "别戳我！"
    }
    
    local index = math.random(1, #greetings)
    pet.say(greetings[index])
end

-- Called on key press
function onKeyPress(key)
    pet.log("Key pressed: " .. key)
    
    -- Special key handlers
    if key == "N" then
        pet.say("打开记事本...")
        pet.openProgram("notepad.exe")
    elseif key == "C" then
        pet.say("打开计算器...")
        pet.openProgram("calc.exe")
    elseif key == "T" then
        pet.say("当前时间: " .. pet.getTime())
    elseif key == "H" then
        showHelp()
    end
end

-- Called every frame (be careful with performance)
local frameCount = 0
function onUpdate()
    frameCount = frameCount + 1
    
    -- Do something every 300 frames (~5 seconds at 60fps)
    if frameCount >= 300 then
        frameCount = 0
        -- Could trigger random actions here
    end
end

-- Helper function: show help
function showHelp()
    local helpText = [[桌面宠物快捷键:
N - 打开记事本
C - 打开计算器  
T - 显示时间
H - 显示帮助
ESC - 退出]]
    
    pet.showMessage(helpText)
end

-- Example: Schedule a greeting after startup
-- (In real use, you'd implement a timer system)
pet.log("Initialization complete!")
