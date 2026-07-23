--TEST--
function default argument vector 017
--FILE--
<?php
function generated_default_017($left, $right = 53) {
    return $left + $right;
}
echo generated_default_017(92), ':', generated_default_017(92, 54);
--EXPECT--
145:146
