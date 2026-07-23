--TEST--
static property method and constant vector 011
--FILE--
<?php
class GeneratedStatic011 {
    public const OFFSET = 12;
    public static $value = 34;
    public static function add($step) { self::$value += $step; return self::$value + self::OFFSET; }
}
echo GeneratedStatic011::add(5), ':', GeneratedStatic011::add(4), ':', GeneratedStatic011::$value;
--EXPECT--
51:55:43
