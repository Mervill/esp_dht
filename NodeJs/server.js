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
}

const templates = {
    //index: fs.readFileSync(`${DHTAppConfig.TemplateDir}/index.html`, 'utf8')
    index: fs.readFileSync(path.join(DHTAppConfig.TemplateDir, 'index.html'), 'utf8')
}

const graphDataLength = (2 * 60 * 24) // 2880

let tempatureDataSet = []

let logStream = null;

const app = express()

app.use('/static', express.static(path.join(__dirname, 'static')))
app.use(express.urlencoded({ extended: false }))
app.use(express.json())

app.get('/', (req, res) => {
    res.send(mustache.render(templates.index, {
        chartdata: tempatureDataSet
    }))
})

app.post('/log', (req, res) => {

    let newdata = req.body
    newdata.time = dateFormat(new Date(), "hh:MM:ssTT")

    console.log(JSON.stringify(newdata))

    let logString = `${newdata.time},${newdata.temp}`
    //console.log(logString)
    logStream.write(`${logString}\n`)

    tempatureDataSet.push(newdata)
    while (tempatureDataSet.length > graphDataLength)
        tempatureDataSet.shift()

    let now = new Date()
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

function SetLogStream()
{
    if (logStream != null && logStream.writeable)
    {
        logStream.end()
    }

    let dateCode = dateFormat(new Date(),"dd-mm-yy")
    let logName = `log-${dateCode}.txt`
    logStream = fs.createWriteStream(logName, {flags:'a'})

    logStream.write(`${dateFormat(new Date(),"dd/mm/yy @ hh:MM:ssTT")},\n`)
    logStream.write(`time,temperature\n`)

    console.log(`Log file is now ${logName}`)
    console.log(`Log rollover at ${dateFormat(new Date(), "hh:MM:ssTT - dd/mm/yy")}`)
}

app.listen(DHTAppConfig.HTTPPort, () => {
    
    SetLogStream();
    schedule.scheduleJob('0 0 * * *', () => {
        SetLogStream();
    })

    console.log(`app listening on port ${DHTAppConfig.HTTPPort}`)
})