const express = require('express')
const mustache = require('mustache')
const dateFormat = require('dateformat')
const schedule = require('node-schedule')
const path = require('path')
const fs = require('fs')
const os = require('os')

const DHTAppConfig = {
    HTTPPort: 8888,
    TemplateDir: path.join(__dirname, 'template'),
    HotReloadTemplates: true,
    LogDir: path.join(__dirname, 'log'), // LogDir: path.join(__dirname, 'static', 'log')
}

const templatePaths = {
    index: path.join(DHTAppConfig.TemplateDir, 'index.html')
}

const templateData = { }

for (const [key, value] of Object.entries(templatePaths))
{
    let keyName = key
    let pathName = value
    templateData[keyName] = fs.readFileSync(pathName, 'utf8')
    console.log(`loaded template '${keyName}' file ${pathName}`)
    if (DHTAppConfig.HotReloadTemplates)
    {
        fs.watch(pathName, 'utf8', (event, filename) => {
            templateData[keyName] = fs.readFileSync(pathName, 'utf8')
            console.log(`reloaded template '${keyName}' file ${pathName}`)
        })
    }
}

const graphDataLength = (2 * 60 * 24) // 2880

let NodeRegistry = []

function FindNodeData(nickname, lazyCreate)
{
    for (const node of NodeRegistry)
    {
        if (node.Nickname == nickname)
        {
            return node;
        }
    }

    if (!lazyCreate)
    {
        console.log(`Didn't find node data for nickname ${nickname}`)    
        return null
    }

    // didn't find a node with that nickname, so create one
    let newNodeData = {
        Nickname: nickname,
        DisplayGraphData: [],
        AquireLogStream: NodeData_AquireLogStream,
    }
    newNodeData.AquireLogStream()
    NodeRegistry.push(newNodeData)

    console.log(`Created new node data for nickname ${nickname}`)

    return newNodeData;
}

function NodeData_AquireLogStream()
{
    if (this.logStream != null && this.logStream.writeable)
    {
        this.logStream.end()
    }

    let dateCode = dateFormat(new Date(),"dd-mm-yy")
    let logName = `log-${this.Nickname}-${dateCode}.txt`
    let logPath = path.join(DHTAppConfig.LogDir, logName)

    let fileIsNew = !fs.existsSync(logPath)
    this.logStream = fs.createWriteStream(logPath, {flags:'a'})

    if (fileIsNew)
    {
        //this.logStream.write(`${this.Nickname},${dateFormat(new Date(),"dd/mm/yy @ hh:MM:ssTT")}\n`)
        this.logStream.write(`${this.Nickname},${dateCode}\n`)
        this.logStream.write(`time,temp,humi\n`)
    }

    let fileState = (fileIsNew) ? "created" : "opened"
    console.log(`Aquired log file ${logName} (${fileState})`)
}

const app = express()

app.use('/static', express.static(path.join(__dirname, 'static')))
app.use(express.urlencoded({ extended: false }))
app.use(express.json())

app.get('/', (req, res) => {
    
    let lastReadings = []
    for (const node of NodeRegistry)
    {
        let reading = node.DisplayGraphData[node.DisplayGraphData.length - 1]
        lastReadings.push(reading)
    }

    res.send(mustache.render(templateData.index, {
        Bedroom: FindNodeData("Bedroom").DisplayGraphData,
        Mainroom: FindNodeData("Mainroom").DisplayGraphData,
        NodeData: NodeRegistry,
        Last: lastReadings
    }))
})

app.post('/log', (req, res) => {
    
    let now = new Date()
    let packet = req.body
    // {"nick":"Mainroom","temp":26,"humi":44.9,"rssi":-41,"time":"04:06:01PM"}

    packet.time = now.getTime()
    packet.log_time = dateFormat(now, "hh:MM:ssTT")

    let req_ip = req.ip
    let ipv6_ends = (req_ip.lastIndexOf(":") + 1)
    packet.ip = req_ip.slice(ipv6_ends)
    
    let nickname = packet.nick
    let nodeData = FindNodeData(nickname, true)

    let logString = `${packet.log_time},${packet.temp},${packet.humi},${packet.rssi}`
    nodeData.logStream.write(`${logString}\n`)

    let graphData = nodeData.DisplayGraphData
    graphData.push(packet)
    while (graphData.length > graphDataLength)
        graphData.shift()

    res.send(`<pre>${dateFormat(now, "hh:MM:ss TT - dd/mm/yy")}</pre>`)
})

app.get('/now', (req, res) => {
    let now = new Date()
    res.send(`<pre>${dateFormat(now, "hh:MM:ss TT - dd/mm/yy")}</pre>`)
})

app.get('/id', (req, res) => {
    const machineInfo = {
        hostname: os.hostname(),
        platform: os.platform(),
        arch: os.arch(),
        uptime: os.uptime(),
        loadavg: os.loadavg(),
        cpus: os.cpus(),
        networkInterfaces: os.networkInterfaces(),
    }
    res.send(`<pre>${JSON.stringify(machineInfo, undefined, 2)}</pre>`)
})

app.listen(DHTAppConfig.HTTPPort, () => {
    
    if (!fs.existsSync(DHTAppConfig.LogDir))
        fs.mkdirSync(DHTAppConfig.LogDir);

    schedule.scheduleJob('0 0 * * *', () => {
        console.log(`Log rollover at ${dateFormat(new Date(), "hh:MM:ssTT - dd/mm/yy")}`)
        for (const node of NodeRegistry)
        {
            node.AquireLogStream()
        }
    })

    console.log(`app listening on port ${DHTAppConfig.HTTPPort}`)
})