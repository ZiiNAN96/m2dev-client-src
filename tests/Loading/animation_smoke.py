"""Inserted into the offline map probe only by -AnimationSmoke."""
import json

class AnimationWorld(World):
    smoke = None

    def next_map(self):
        if self.index + 1 < len(VIEWS):
            return World.next_map(self)
        self.smoke = -1
        self.steps = [('player', 57900, name, motion) for name, motion in (
            ('Idle', chr.MOTION_WAIT), ('Walk', chr.MOTION_WALK), ('Run', chr.MOTION_RUN),
            ('Attack', chr.MOTION_COMBO_ATTACK_1), ('Damage', chr.MOTION_DAMAGE), ('Death', chr.MOTION_DEAD))]
        self.steps += [('mob', 57902, name, motion) for name, motion in (
            ('Idle', chr.MOTION_WAIT), ('Walk', chr.MOTION_WALK), ('Run', chr.MOTION_RUN),
            ('Attack', chr.MOTION_NORMAL_ATTACK), ('Damage', chr.MOTION_DAMAGE), ('Death', chr.MOTION_DEAD))]
        self.rows = []
        self.advance_smoke()

    def advance_smoke(self):
        if self.smoke >= 0:
            app.MapLoadTrace('pop')
            app.MapLoadTrace('end', 'animation-smoke-complete')
            self.rows.append(self.row)
        self.smoke += 1
        if self.smoke == len(self.steps):
            with builtins.old_open('animation-smoke.json', 'w') as output:
                json.dump(self.rows, output, indent=2)
            self.index = len(VIEWS)
            app.Exit()
            return
        actor, vid, name, motion = self.steps[self.smoke]
        self.row = dict(actor=actor, motion=name, index=motion, max_frame_ms=0, frames=0)
        app.MapLoadTrace('begin', 'animation-smoke-' + actor + '-' + name)
        app.MapLoadTrace('push', 'animation smoke observation')
        chr.SelectInstance(vid)
        start = time.monotonic()
        # Locomotion uses loops; combat/reactions use the normal once queue.
        if name in ('Idle', 'Walk', 'Run'):
            chr.SetLoopMotion(motion)
        else:
            chr.PushOnceMotion(motion, 0.0)
        self.row['request_ms'] = (time.monotonic() - start) * 1000
        self.smoke_started = start
        self.last_update = start
        self.smoke_shot = False

    def OnUpdate(self):
        if self.smoke is None:
            return World.OnUpdate(self)
        if self.index >= len(VIEWS): return
        now = time.monotonic()
        self.row['max_frame_ms'] = max(self.row['max_frame_ms'], (now - self.last_update) * 1000)
        self.row['frames'] += 1
        self.last_update = now
        if now - self.smoke_started >= 1.0:
            self.advance_smoke()
            if self.index >= len(VIEWS): return
        x, y, z = self.position
        app.SetCenterPosition(x, -y, z + 100)
        app.SetCamera(4500.0, 25.0, 0.0, 0.0)
        background.Update(x, -y, z)
        chr.Update()
        effect.Update()

    def OnRender(self):
        World.OnRender(self)
        if self.smoke is not None and self.index < len(VIEWS) and not self.smoke_shot and time.monotonic() - self.smoke_started > .35:
            # Exclude capture I/O from the subsequent frame interval.
            if self.row['motion'] in ('Attack', 'Death'):
                ok, path = grp.SaveScreenShotToPath('animation-' + self.row['actor'] + '-' + self.row['motion'] + '-')
                if not ok: raise RuntimeError('Animation screenshot failed')
            self.smoke_shot = True
            self.last_update = time.monotonic()
