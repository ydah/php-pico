--TEST--
finally runs in a closure returned by a method
--FILE--
<?php
class FinalClosureFactory {
    public function make($base) {
        return function ($step) use ($base) {
            try { return $base + $step; } finally { echo 'finally:'; }
        };
    }
}
$callback = (new FinalClosureFactory())->make(30); echo $callback(12);
--EXPECT--
finally:42
