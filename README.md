# macOS CTF challenges
This repository contains a set of macOS-specific CTF challenges.  
At the time, LLMs such as [Claude Opus](https://www.anthropic.com/claude/opus) were not dealing with some of them well, and got "stuck".  
Personally, I believe it is because those models have very little concept of how a bigger system works - thus, self-contained challenges are relatively "easy", while challenges that involve external or unexpected OS behavior (as is usually the case in macOS!) pose a challenge.  

## Structure
All challenges contain a special `prepare.sh` file which sets up the challenge under `/Users/ctf/`.  
A flag is also placed under `/Users/ctf/flag` which is only root-readable.  
The goal in all those challenges would be reading the flag.  
I have also provided solutions under `./solution` directory for each challenge - note how simple those usually are.
