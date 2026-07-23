--TEST--
static property method and constant vector 004
--FILE--
<?php
class GeneratedStatic004 {
    public const OFFSET = 5;
    public static $value = 13;
    public static function add($step) { self::$value += $step; return self::$value + self::OFFSET; }
}
echo GeneratedStatic004::add(2), ':', GeneratedStatic004::add(7), ':', GeneratedStatic004::$value;
--EXPECT--
20:27:22
