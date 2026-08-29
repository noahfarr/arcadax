import ctypes
import dataclasses

from .dsl import DslGame
from .validate import explore


def _wins(game) -> bool:
    sym = game.library.sym
    return (game.state == "WIN"
            or int(sym.harness_level_index(game.handle)) > 0)


def transfers(spec_a, spec_b, aux_size: int, max_nodes: int = 80_000,
              library=None) -> dict:
    hybrid = dataclasses.replace(spec_b, rules=spec_a.rules)
    g = DslGame(hybrid, library=library)
    plan = explore(g, aux_size, max_nodes=max_nodes)
    g.close()
    if not plan.solvable or plan.path is None:
        return {"transfers": False, "reason": "no plan under the other rules"}

    g = DslGame(spec_b, library=library)
    g.init()
    for action in plan.path:
        g.act(*action)
        if g.state == "GAME_OVER":
            break
    won = _wins(g)
    g.close()
    return {"transfers": bool(won), "plan_length": len(plan.path),
            "reason": "plan executed" if won else "plan failed in the real rules"}


def pairwise(specs, aux_size: int, library=None) -> list[dict]:
    out = []
    for i in range(len(specs)):
        for j in range(len(specs)):
            if i == j:
                continue
            r = transfers(specs[i], specs[j], aux_size, library=library)
            out.append({"from": i, "to": j, **r})
    return out


def _gate_kinds(spec):
    gates = []
    for r in spec.rules:
        if r.trigger == 1:
            gates.append((r.effect_a, r.subject))
        elif r.subject >= 0:
            gates.append((r.effect_a, r.subject))
        else:
            gates.append((r.effect_a, r.pred_a))
    return gates


def _variant(spec, assignment):
    from .dsl import (IF_NONE_LEFT, NONE, ON_CLICK, ON_ENTER, ON_STEP, REMOVE,
                      Rule)

    gates = _gate_kinds(spec)
    kinds = [dataclasses.replace(k) for k in spec.kinds]
    rules = []
    for (door, trigger), choice in zip(gates, assignment):
        if choice == "key":
            kinds[trigger].on_enter = REMOVE
            rules.append(Rule(trigger=ON_ENTER, subject=trigger, effect=5,
                              effect_a=door, effect_b=0))
        elif choice == "switch":
            kinds[trigger].on_enter = NONE
            rules.append(Rule(trigger=ON_CLICK, subject=trigger, effect=5,
                              effect_a=door, effect_b=0))
        else:
            kinds[trigger].on_enter = REMOVE
            rules.append(Rule(trigger=ON_STEP, subject=-1, effect=5,
                              predicate=IF_NONE_LEFT, pred_a=trigger,
                              effect_a=door, effect_b=0))
    return dataclasses.replace(spec, kinds=kinds, rules=rules)


def identifiability(spec, aux_size: int, actions, library=None) -> dict:
    import itertools

    import numpy as np

    gates = _gate_kinds(spec)
    space = list(itertools.product(("key", "switch", "collect"),
                                   repeat=len(gates)))
    truth = DslGame(spec, library=library)
    truth.init()
    observed = [truth.act(*a)[-1].copy() for a in actions]
    truth.close()

    alive = []
    for assignment in space:
        g = DslGame(_variant(spec, assignment), library=library)
        g.init()
        frames = []
        for a in actions:
            out = g.act(*a)
            frames.append(out[-1].copy() if out else None)
        g.close()
        alive.append(frames)

    survivors = []
    live = list(range(len(space)))
    for step in range(len(actions)):
        live = [i for i in live
                if alive[i][step] is not None
                and np.array_equal(alive[i][step], observed[step])]
        survivors.append(len(live))
        if len(live) <= 1:
            break
    return {"hypotheses": len(space), "survivors": survivors,
            "identified_after": len(survivors) if survivors[-1] <= 1 else None}
