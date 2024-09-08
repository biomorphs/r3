Fastqueue = {}

-- adapted from https://www.lua.org/pil/11.4.html

function Fastqueue.new ()
    return {first = 0, last = -1}
end

function Fastqueue.pushleft (list, value)
      local first = list.first - 1
      list.first = first
      list[first] = value
end
    
function Fastqueue.pushright (list, value)
    local last = list.last + 1
    list.last = last
    list[last] = value
end
    
function Fastqueue.popleft (list)
    local first = list.first
    if first > list.last then error("list is empty") end
    local value = list[first]
    list[first] = nil        -- to allow garbage collection
    list.first = first + 1
    return value
end
    
function Fastqueue.popright (list)
    local last = list.last
    if list.first > last then error("list is empty") end
    local value = list[last]
    list[last] = nil         -- to allow garbage collection
    list.last = last - 1
    return value
end