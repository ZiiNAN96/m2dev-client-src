"""Bounded first/second/third playback of the SAME registered clip, after A1."""
import chrmgr, json
LOAD_PREWARM = False

class FirstUseWorld(World):
    use = None

    def next_map(self):
        if self.index + 1 < len(VIEWS):
            World.next_map(self)
            self.prewarm_pending = LOAD_PREWARM
            return
        # Pin existing random alternatives; never change asset data/registration.
        # These counts are the real A1 race/motion registrations.
        for race, mode, counts in (
            (0, chr.MOTION_MODE_ONEHAND_SWORD, ((chr.MOTION_WAIT, 2), (chr.MOTION_DAMAGE, 2))),
            (101, chr.MOTION_MODE_GENERAL, ((chr.MOTION_WAIT, 3), (chr.MOTION_NORMAL_ATTACK, 2)) )):
            chrmgr.SelectRace(race)
            for motion, count in counts:
                for sub in range(count):
                    chrmgr.SetMotionRandomWeight(mode, motion, sub, 100 if sub == 0 else 0)
        self.steps = []
        for actor, vid, attack in (('player', 57900, chr.MOTION_COMBO_ATTACK_1), ('mob', 57902, chr.MOTION_NORMAL_ATTACK)):
            for name, motion in (('Idle', chr.MOTION_WAIT), ('Walk', chr.MOTION_WALK), ('Run', chr.MOTION_RUN),
                                 ('Attack', attack), ('Damage', chr.MOTION_DAMAGE), ('Death', chr.MOTION_DEAD)):
                for use in (1, 2, 3):
                    self.steps.append((actor, vid, name, motion, use))
        self.rows = []
        self.use = -1
        self.advance_use()

    def advance_use(self):
        if self.use >= 0:
            app.MapLoadTrace('pop')
            app.MapLoadTrace('end', 'first-use-complete')
            self.rows.append(self.row)
            # Clear the once queue without destroying instances or clip caches.
            chr.SelectInstance(self.steps[self.use][1])
            chr.SetLoopMotion(chr.MOTION_WAIT)
        self.use += 1
        if self.use == len(self.steps):
            with builtins.old_open('animation-first-use.json', 'w') as output:
                json.dump(self.rows, output, indent=2)
            self.index = len(VIEWS)
            app.Exit()
            return
        actor, vid, name, motion, use = self.steps[self.use]
        self.row = dict(actor=actor, motion=name, index=motion, use=use, max_frame_ms=0, frames=0, max_update_ms=0)
        app.MapLoadTrace('begin', 'first-use-%s-%s-%d' % (actor, name, use))
        app.MapLoadTrace('push', 'first use observation')
        chr.SelectInstance(vid)
        start = time.monotonic()
        if name in ('Idle', 'Walk', 'Run'):
            chr.SetLoopMotion(motion)
        else:
            chr.PushOnceMotion(motion, 0.0)
        self.row['request_ms'] = (time.monotonic() - start) * 1000
        self.use_started = start
        self.last_update = start

    def OnUpdate(self):
        if self.use is None:
            return World.OnUpdate(self)
        if self.index >= len(VIEWS): return
        now = time.monotonic()
        self.row['max_frame_ms'] = max(self.row['max_frame_ms'], (now - self.last_update) * 1000)
        self.row['frames'] += 1
        self.last_update = now
        if now - self.use_started >= .35:
            self.advance_use()
            if self.index >= len(VIEWS): return
        start = time.monotonic()
        x, y, z = self.position
        app.SetCenterPosition(x, -y, z + 100)
        app.SetCamera(4500.0, 25.0, 0.0, 0.0)
        background.Update(x, -y, z)
        chr.Update()
        effect.Update()
        self.row['max_update_ms'] = max(self.row['max_update_ms'], (time.monotonic() - start) * 1000)

    def OnRender(self):
        if getattr(self, 'prewarm_pending', False):
            self.prewarm_pending = False
            start = time.monotonic()
            with Phase('Actors'):
                if not chrmgr.PrewarmVisibleActors(True):
                    raise RuntimeError('Production loading preparation failed')
            log.write('production-prewarm-ms=%.6f\n' % ((time.monotonic()-start)*1000))
            log.flush()
        World.OnRender(self)
