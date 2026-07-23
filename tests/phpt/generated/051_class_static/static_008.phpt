--TEST--
static property method and constant vector 008
--FILE--
<?php
class GeneratedStatic008 {
    public const OFFSET = 9;
    public static $value = 25;
    public static function add($step) { self::$value += $step; return self::$value + self::OFFSET; }
}
echo GeneratedStatic008::add(2), ':', GeneratedStatic008::add(6), ':', GeneratedStatic008::$value;
--EXPECT--
36:42:33
