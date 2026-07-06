# Experiential Learning of Runtime Monitoring Using Pachinko

We present documentation of a classroom assignment that teaches runtime monitoring through a creative embedded systems build: an interactive Pachinko game. The assignment centers on a dual-core ESP32 workflow in which students write RTLola specifications for monitors, compile these monitors to C, and deploy them alongside sensor and actuator control logic. Pachinko game events are logged in real time and used to trigger sound, animation, and motor behavior according to formal temporal logic specifications.

This work showcases how formal methods can be taught in a hands-on, project-based setting for learners in a creative and classroom-scale setting. We also discuss portability: the assignment template, hardware stack, code base, and assessment approach are designed and documented to be replicated in other embedded systems, creative computing, or makerspace-style courses. This assignment was given to the students of Creative Embedded Systems (COMS3930) at Barnard College.

## Demo Video

<!--
  GitHub README does not render <iframe> embeds. Use one of the options below.

  Option A (recommended for GitHub): clickable thumbnail that opens YouTube.
  Replace VIDEO_ID with your YouTube video ID (the part after v= in the URL).
-->

[![Pachinko board demo](https://img.youtube.com/vi/MDFKCMT7R9U/maxresdefault.jpg)](https://www.youtube.com/watch?v=MDFKCMT7R9U)

**Direct link:** [Watch on YouTube](https://www.youtube.com/watch?v=MDFKCMT7R9U)

<!--
  Option B: if you host this README elsewhere (e.g. a project website) that
  supports iframes, you can use:

  <iframe width="560" height="315"
    src="https://www.youtube.com/embed/VIDEO_ID"
    title="Pachinko runtime monitoring demo"
    frameborder="0"
    allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share"
    allowfullscreen>
  </iframe>
-->

---

# Ball logger and monitor ESP32

![Pachinko board — overview](<Pachinko Board Images/Pachinko Example Bottom.jpg>)

### Student projects

![Student board 1](<Pachinko Board Images/Student Pachinko Boards.png>)

---

## Repository contents

- RTLola PlatformIO project folder that can run on ESP32 (does not include RTLola2C compiler)
- 3D models for pachinko ball logger, stepper motor mount, and motor arms
- Some images of an example pachinko board and wiring as well as boards made by students
- Full paper describing the assignment and its context

If you wish to gain access to the RTLola2C compiler to replicate this assignemnt, please contact Miles Scharff or Mark Santolucito

---

