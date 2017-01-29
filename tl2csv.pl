#!/usr/bin/perl

my $threadId, $burstStart, $nextPreemption;

print("ThreadId,BurstStart(uS),NextPreemption(uS)\n");
while(<>)
{
    if (/cur = \(volatile miosix::Thread \*\) (\dx[0-9abcdef]+)/){
        $threadId = $1;
    }
    if (/nextPreemption = (\d+)/){
        $nextPreemption = $1/1000;
    }
    if (/burstStart = (\d+)/){
        $burstStart = $1/1000;
        print("$threadId,$burstStart,$nextPreemption\n");
    }
}
