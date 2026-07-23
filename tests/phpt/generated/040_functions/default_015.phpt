--TEST--
function default argument vector 015
--FILE--
<?php
function generated_default_015($left, $right = 47) {
    return $left + $right;
}
echo generated_default_015(82), ':', generated_default_015(82, 48);
--EXPECT--
129:130
