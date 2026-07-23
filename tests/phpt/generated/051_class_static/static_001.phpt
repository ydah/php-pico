--TEST--
static property method and constant vector 001
--FILE--
<?php
class GeneratedStatic001 {
    public const OFFSET = 2;
    public static $value = 4;
    public static function add($step) { self::$value += $step; return self::$value + self::OFFSET; }
}
echo GeneratedStatic001::add(3), ':', GeneratedStatic001::add(4), ':', GeneratedStatic001::$value;
--EXPECT--
9:13:11
