--TEST--
static property method and constant vector 003
--FILE--
<?php
class GeneratedStatic003 {
    public const OFFSET = 4;
    public static $value = 10;
    public static function add($step) { self::$value += $step; return self::$value + self::OFFSET; }
}
echo GeneratedStatic003::add(5), ':', GeneratedStatic003::add(6), ':', GeneratedStatic003::$value;
--EXPECT--
19:25:21
