// Code generated by protoc-gen-go.
// source: IM.Chat.proto
// DO NOT EDIT!

package youmai

import proto "github.com/golang/protobuf/proto"
import fmt "fmt"
import math "math"

// Reference imports to suppress errors if they are not otherwise used.
var _ = proto.Marshal
var _ = fmt.Errorf
var _ = math.Inf

type IM_CHANNEL int32

const (
	IM_CHANNEL_IM_CHANNEL_DEFAULT IM_CHANNEL = 0
	IM_CHANNEL_IM_CHANNEL_SMS     IM_CHANNEL = 1
	IM_CHANNEL_IM_CHANNEL_QQ      IM_CHANNEL = 2
)

var IM_CHANNEL_name = map[int32]string{
	0: "IM_CHANNEL_DEFAULT",
	1: "IM_CHANNEL_SMS",
	2: "IM_CHANNEL_QQ",
}
var IM_CHANNEL_value = map[string]int32{
	"IM_CHANNEL_DEFAULT": 0,
	"IM_CHANNEL_SMS":     1,
	"IM_CHANNEL_QQ":      2,
}

func (x IM_CHANNEL) Enum() *IM_CHANNEL {
	p := new(IM_CHANNEL)
	*p = x
	return p
}
func (x IM_CHANNEL) String() string {
	return proto.EnumName(IM_CHANNEL_name, int32(x))
}
func (x *IM_CHANNEL) UnmarshalJSON(data []byte) error {
	value, err := proto.UnmarshalJSONEnum(IM_CHANNEL_value, data, "IM_CHANNEL")
	if err != nil {
		return err
	}
	*x = IM_CHANNEL(value)
	return nil
}
func (IM_CHANNEL) EnumDescriptor() ([]byte, []int) { return fileDescriptor32, []int{0} }

type IM_CONTENT_TYPE int32

const (
	IM_CONTENT_TYPE_IM_CONTENT_TYPE_TEXT          IM_CONTENT_TYPE = 0
	IM_CONTENT_TYPE_IM_CONTENT_TYPE_SHORT_MESSAGE IM_CONTENT_TYPE = 1
	IM_CONTENT_TYPE_IM_CONTENT_TYPE_CONTACTS      IM_CONTENT_TYPE = 2
	IM_CONTENT_TYPE_IM_CONTENT_TYPE_RECOMMEND_APP IM_CONTENT_TYPE = 3
	IM_CONTENT_TYPE_IM_CONTENT_TYPE_NO_DISTURB    IM_CONTENT_TYPE = 4
	IM_CONTENT_TYPE_IM_CONTENT_TYPE_LOCATIONSHARE IM_CONTENT_TYPE = 5
	IM_CONTENT_TYPE_IM_CONTENT_TYPE_IMAGE         IM_CONTENT_TYPE = 6
	IM_CONTENT_TYPE_IM_CONTENT_TYPE_AT            IM_CONTENT_TYPE = 7
	IM_CONTENT_TYPE_IM_CONTENT_TYPE_URL           IM_CONTENT_TYPE = 8
	IM_CONTENT_TYPE_IM_CONTENT_TYPE_AUDIO         IM_CONTENT_TYPE = 9
	IM_CONTENT_TYPE_IM_CONTENT_TYPE_VIDEO         IM_CONTENT_TYPE = 10
	IM_CONTENT_TYPE_IM_CONTENT_TYPE_LOCATION      IM_CONTENT_TYPE = 11
	IM_CONTENT_TYPE_IM_CONTENT_TYPE_FILE          IM_CONTENT_TYPE = 12
	IM_CONTENT_TYPE_IM_CONTENT_TYPE_BIZCARD       IM_CONTENT_TYPE = 13
	IM_CONTENT_TYPE_IM_CONTENT_TYPE_KAQUAN        IM_CONTENT_TYPE = 14
)

var IM_CONTENT_TYPE_name = map[int32]string{
	0:  "IM_CONTENT_TYPE_TEXT",
	1:  "IM_CONTENT_TYPE_SHORT_MESSAGE",
	2:  "IM_CONTENT_TYPE_CONTACTS",
	3:  "IM_CONTENT_TYPE_RECOMMEND_APP",
	4:  "IM_CONTENT_TYPE_NO_DISTURB",
	5:  "IM_CONTENT_TYPE_LOCATIONSHARE",
	6:  "IM_CONTENT_TYPE_IMAGE",
	7:  "IM_CONTENT_TYPE_AT",
	8:  "IM_CONTENT_TYPE_URL",
	9:  "IM_CONTENT_TYPE_AUDIO",
	10: "IM_CONTENT_TYPE_VIDEO",
	11: "IM_CONTENT_TYPE_LOCATION",
	12: "IM_CONTENT_TYPE_FILE",
	13: "IM_CONTENT_TYPE_BIZCARD",
	14: "IM_CONTENT_TYPE_KAQUAN",
}
var IM_CONTENT_TYPE_value = map[string]int32{
	"IM_CONTENT_TYPE_TEXT":          0,
	"IM_CONTENT_TYPE_SHORT_MESSAGE": 1,
	"IM_CONTENT_TYPE_CONTACTS":      2,
	"IM_CONTENT_TYPE_RECOMMEND_APP": 3,
	"IM_CONTENT_TYPE_NO_DISTURB":    4,
	"IM_CONTENT_TYPE_LOCATIONSHARE": 5,
	"IM_CONTENT_TYPE_IMAGE":         6,
	"IM_CONTENT_TYPE_AT":            7,
	"IM_CONTENT_TYPE_URL":           8,
	"IM_CONTENT_TYPE_AUDIO":         9,
	"IM_CONTENT_TYPE_VIDEO":         10,
	"IM_CONTENT_TYPE_LOCATION":      11,
	"IM_CONTENT_TYPE_FILE":          12,
	"IM_CONTENT_TYPE_BIZCARD":       13,
	"IM_CONTENT_TYPE_KAQUAN":        14,
}

func (x IM_CONTENT_TYPE) Enum() *IM_CONTENT_TYPE {
	p := new(IM_CONTENT_TYPE)
	*p = x
	return p
}
func (x IM_CONTENT_TYPE) String() string {
	return proto.EnumName(IM_CONTENT_TYPE_name, int32(x))
}
func (x *IM_CONTENT_TYPE) UnmarshalJSON(data []byte) error {
	value, err := proto.UnmarshalJSONEnum(IM_CONTENT_TYPE_value, data, "IM_CONTENT_TYPE")
	if err != nil {
		return err
	}
	*x = IM_CONTENT_TYPE(value)
	return nil
}
func (IM_CONTENT_TYPE) EnumDescriptor() ([]byte, []int) { return fileDescriptor32, []int{1} }

type IMChat_Personal struct {
	CommandId      *int32  `protobuf:"varint,1,opt,name=command_id,json=commandId" json:"command_id,omitempty"`
	Timestamp      *int32  `protobuf:"varint,2,opt,name=timestamp" json:"timestamp,omitempty"`
	SendCrc        *int32  `protobuf:"varint,3,opt,name=send_crc,json=sendCrc" json:"send_crc,omitempty"`
	Version        *int32  `protobuf:"varint,4,opt,name=version" json:"version,omitempty"`
	MsgId          *int64  `protobuf:"varint,5,opt,name=msg_id,json=msgId" json:"msg_id,omitempty"`
	SrcUsrId       *int32  `protobuf:"varint,6,opt,name=src_usr_id,json=srcUsrId" json:"src_usr_id,omitempty"`
	SrcPhone       *string `protobuf:"bytes,7,opt,name=src_phone,json=srcPhone" json:"src_phone,omitempty"`
	SrcName        *string `protobuf:"bytes,8,opt,name=src_name,json=srcName" json:"src_name,omitempty"`
	SrcUserType    *int32  `protobuf:"varint,9,opt,name=src_user_type,json=srcUserType" json:"src_user_type,omitempty"`
	TargetUserId   *int32  `protobuf:"varint,10,opt,name=target_user_id,json=targetUserId" json:"target_user_id,omitempty"`
	TargetPhone    *string `protobuf:"bytes,11,opt,name=target_phone,json=targetPhone" json:"target_phone,omitempty"`
	TargetName     *string `protobuf:"bytes,12,opt,name=target_name,json=targetName" json:"target_name,omitempty"`
	TargetUserType *int32  `protobuf:"varint,13,opt,name=target_user_type,json=targetUserType" json:"target_user_type,omitempty"`
	// 0:transmit 1:not transmit
	ImChannel        *IM_CHANNEL `protobuf:"varint,14,opt,name=im_channel,json=imChannel,enum=com.proto.chat.IM_CHANNEL" json:"im_channel,omitempty"`
	Body             *string     `protobuf:"bytes,15,opt,name=body" json:"body,omitempty"`
	ContentType      *int32      `protobuf:"varint,16,opt,name=content_type,json=contentType" json:"content_type,omitempty"`
	Parentid         *int32      `protobuf:"varint,17,opt,name=parentid" json:"parentid,omitempty"`
	XXX_unrecognized []byte      `json:"-"`
}

func (m *IMChat_Personal) Reset()                    { *m = IMChat_Personal{} }
func (m *IMChat_Personal) String() string            { return proto.CompactTextString(m) }
func (*IMChat_Personal) ProtoMessage()               {}
func (*IMChat_Personal) Descriptor() ([]byte, []int) { return fileDescriptor32, []int{0} }

func (m *IMChat_Personal) GetCommandId() int32 {
	if m != nil && m.CommandId != nil {
		return *m.CommandId
	}
	return 0
}

func (m *IMChat_Personal) GetTimestamp() int32 {
	if m != nil && m.Timestamp != nil {
		return *m.Timestamp
	}
	return 0
}

func (m *IMChat_Personal) GetSendCrc() int32 {
	if m != nil && m.SendCrc != nil {
		return *m.SendCrc
	}
	return 0
}

func (m *IMChat_Personal) GetVersion() int32 {
	if m != nil && m.Version != nil {
		return *m.Version
	}
	return 0
}

func (m *IMChat_Personal) GetMsgId() int64 {
	if m != nil && m.MsgId != nil {
		return *m.MsgId
	}
	return 0
}

func (m *IMChat_Personal) GetSrcUsrId() int32 {
	if m != nil && m.SrcUsrId != nil {
		return *m.SrcUsrId
	}
	return 0
}

func (m *IMChat_Personal) GetSrcPhone() string {
	if m != nil && m.SrcPhone != nil {
		return *m.SrcPhone
	}
	return ""
}

func (m *IMChat_Personal) GetSrcName() string {
	if m != nil && m.SrcName != nil {
		return *m.SrcName
	}
	return ""
}

func (m *IMChat_Personal) GetSrcUserType() int32 {
	if m != nil && m.SrcUserType != nil {
		return *m.SrcUserType
	}
	return 0
}

func (m *IMChat_Personal) GetTargetUserId() int32 {
	if m != nil && m.TargetUserId != nil {
		return *m.TargetUserId
	}
	return 0
}

func (m *IMChat_Personal) GetTargetPhone() string {
	if m != nil && m.TargetPhone != nil {
		return *m.TargetPhone
	}
	return ""
}

func (m *IMChat_Personal) GetTargetName() string {
	if m != nil && m.TargetName != nil {
		return *m.TargetName
	}
	return ""
}

func (m *IMChat_Personal) GetTargetUserType() int32 {
	if m != nil && m.TargetUserType != nil {
		return *m.TargetUserType
	}
	return 0
}

func (m *IMChat_Personal) GetImChannel() IM_CHANNEL {
	if m != nil && m.ImChannel != nil {
		return *m.ImChannel
	}
	return IM_CHANNEL_IM_CHANNEL_DEFAULT
}

func (m *IMChat_Personal) GetBody() string {
	if m != nil && m.Body != nil {
		return *m.Body
	}
	return ""
}

func (m *IMChat_Personal) GetContentType() int32 {
	if m != nil && m.ContentType != nil {
		return *m.ContentType
	}
	return 0
}

func (m *IMChat_Personal) GetParentid() int32 {
	if m != nil && m.Parentid != nil {
		return *m.Parentid
	}
	return 0
}

type IMChat_Personal_Notify struct {
	Imchat           *IMChat_Personal `protobuf:"bytes,1,opt,name=imchat" json:"imchat,omitempty"`
	XXX_unrecognized []byte           `json:"-"`
}

func (m *IMChat_Personal_Notify) Reset()                    { *m = IMChat_Personal_Notify{} }
func (m *IMChat_Personal_Notify) String() string            { return proto.CompactTextString(m) }
func (*IMChat_Personal_Notify) ProtoMessage()               {}
func (*IMChat_Personal_Notify) Descriptor() ([]byte, []int) { return fileDescriptor32, []int{1} }

func (m *IMChat_Personal_Notify) GetImchat() *IMChat_Personal {
	if m != nil {
		return m.Imchat
	}
	return nil
}

type IMChat_Personal_Ack struct {
	SrcUsrId         *int32      `protobuf:"varint,1,opt,name=src_usr_id,json=srcUsrId" json:"src_usr_id,omitempty"`
	MsgId            *int64      `protobuf:"varint,2,opt,name=msg_id,json=msgId" json:"msg_id,omitempty"`
	ErrerNo          *ERRNO_CODE `protobuf:"varint,3,opt,name=errer_no,json=errerNo,enum=com.proto.basic.ERRNO_CODE" json:"errer_no,omitempty"`
	CommandId        *int32      `protobuf:"varint,4,opt,name=command_id,json=commandId" json:"command_id,omitempty"`
	IsTargetOnline   *bool       `protobuf:"varint,5,opt,name=is_target_online,json=isTargetOnline" json:"is_target_online,omitempty"`
	XXX_unrecognized []byte      `json:"-"`
}

func (m *IMChat_Personal_Ack) Reset()                    { *m = IMChat_Personal_Ack{} }
func (m *IMChat_Personal_Ack) String() string            { return proto.CompactTextString(m) }
func (*IMChat_Personal_Ack) ProtoMessage()               {}
func (*IMChat_Personal_Ack) Descriptor() ([]byte, []int) { return fileDescriptor32, []int{2} }

func (m *IMChat_Personal_Ack) GetSrcUsrId() int32 {
	if m != nil && m.SrcUsrId != nil {
		return *m.SrcUsrId
	}
	return 0
}

func (m *IMChat_Personal_Ack) GetMsgId() int64 {
	if m != nil && m.MsgId != nil {
		return *m.MsgId
	}
	return 0
}

func (m *IMChat_Personal_Ack) GetErrerNo() ERRNO_CODE {
	if m != nil && m.ErrerNo != nil {
		return *m.ErrerNo
	}
	return ERRNO_CODE_ERRNO_CODE_OK
}

func (m *IMChat_Personal_Ack) GetCommandId() int32 {
	if m != nil && m.CommandId != nil {
		return *m.CommandId
	}
	return 0
}

func (m *IMChat_Personal_Ack) GetIsTargetOnline() bool {
	if m != nil && m.IsTargetOnline != nil {
		return *m.IsTargetOnline
	}
	return false
}

func init() {
	proto.RegisterType((*IMChat_Personal)(nil), "com.proto.chat.IMChat_Personal")
	proto.RegisterType((*IMChat_Personal_Notify)(nil), "com.proto.chat.IMChat_Personal_Notify")
	proto.RegisterType((*IMChat_Personal_Ack)(nil), "com.proto.chat.IMChat_Personal_Ack")
	proto.RegisterEnum("com.proto.chat.IM_CHANNEL", IM_CHANNEL_name, IM_CHANNEL_value)
	proto.RegisterEnum("com.proto.chat.IM_CONTENT_TYPE", IM_CONTENT_TYPE_name, IM_CONTENT_TYPE_value)
}

func init() { proto.RegisterFile("IM.Chat.proto", fileDescriptor32) }

var fileDescriptor32 = []byte{
	// 730 bytes of a gzipped FileDescriptorProto
	0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0xff, 0x6c, 0x53, 0xcd, 0x6e, 0x9b, 0x4c,
	0x14, 0xfd, 0xf0, 0xbf, 0xaf, 0x63, 0x32, 0x99, 0xaf, 0x49, 0x88, 0x93, 0x34, 0x8e, 0xd5, 0x85,
	0x95, 0x85, 0x17, 0x59, 0xb4, 0xea, 0x92, 0x60, 0xd2, 0xa0, 0x18, 0xb0, 0x01, 0x57, 0x4d, 0x37,
	0x23, 0x02, 0xd3, 0x18, 0x35, 0x80, 0x35, 0x90, 0x4a, 0x7e, 0xb1, 0xbe, 0x41, 0x57, 0x7d, 0xa9,
	0x8a, 0x81, 0xd4, 0x36, 0xf5, 0x6e, 0xee, 0x39, 0x67, 0xee, 0x3d, 0xba, 0x73, 0x06, 0x0e, 0x1e,
	0xe2, 0x17, 0xdd, 0x0d, 0x46, 0xca, 0xc2, 0x4d, 0x47, 0x4b, 0x16, 0xa7, 0x31, 0x16, 0xbd, 0x38,
	0xcc, 0x8f, 0x23, 0x6f, 0xe1, 0xa6, 0x3d, 0x5c, 0x48, 0x6e, 0xdc, 0x24, 0xf0, 0x72, 0x62, 0xf0,
	0xb3, 0x06, 0xfb, 0x9a, 0x9e, 0x5d, 0x22, 0x53, 0xca, 0x92, 0x38, 0x72, 0x9f, 0xf1, 0x39, 0x80,
	0x17, 0x87, 0xa1, 0x1b, 0xf9, 0x24, 0xf0, 0x25, 0xa1, 0x2f, 0x0c, 0xeb, 0x56, 0xbb, 0x40, 0x34,
	0x1f, 0x9f, 0x41, 0x3b, 0x0d, 0x42, 0x9a, 0xa4, 0x6e, 0xb8, 0x94, 0x2a, 0x39, 0xfb, 0x17, 0xc0,
	0x27, 0xd0, 0x4a, 0x68, 0xe4, 0x13, 0x8f, 0x79, 0x52, 0x95, 0x93, 0xcd, 0xac, 0x56, 0x98, 0x87,
	0x25, 0x68, 0xfe, 0xa0, 0x2c, 0x09, 0xe2, 0x48, 0xaa, 0xe5, 0x4c, 0x51, 0xe2, 0x43, 0x68, 0x84,
	0xc9, 0x53, 0x36, 0xad, 0xde, 0x17, 0x86, 0x55, 0xab, 0x1e, 0x26, 0x4f, 0x7c, 0x12, 0x24, 0xcc,
	0x23, 0x2f, 0x09, 0xcb, 0xa8, 0x06, 0xbf, 0xd3, 0x4a, 0x98, 0x37, 0x4f, 0x98, 0xe6, 0xe3, 0x53,
	0x68, 0x67, 0xec, 0x72, 0x11, 0x47, 0x54, 0x6a, 0xf6, 0x85, 0x61, 0x9b, 0x93, 0xd3, 0xac, 0xe6,
	0x36, 0x98, 0x47, 0x22, 0x37, 0xa4, 0x52, 0x8b, 0x73, 0xcd, 0x84, 0x79, 0x86, 0x1b, 0x52, 0x3c,
	0x80, 0x6e, 0xde, 0x95, 0x32, 0x92, 0xae, 0x96, 0x54, 0x6a, 0xf3, 0xc6, 0x1d, 0xde, 0x98, 0x32,
	0x67, 0xb5, 0xa4, 0xf8, 0x1d, 0x88, 0xa9, 0xcb, 0x9e, 0x68, 0x9a, 0xcb, 0x02, 0x5f, 0x02, 0x2e,
	0xda, 0xcb, 0xd1, 0x4c, 0xa7, 0xf9, 0xf8, 0x12, 0x8a, 0xba, 0x30, 0xd1, 0xe1, 0x83, 0x3a, 0x39,
	0x96, 0xfb, 0xb8, 0x80, 0xa2, 0xcc, 0xad, 0xec, 0x71, 0x05, 0xe4, 0x10, 0x77, 0x33, 0x04, 0xb4,
	0x39, 0x89, 0x1b, 0xea, 0xf2, 0x59, 0xe2, 0x7a, 0x16, 0xf7, 0xf4, 0x11, 0x20, 0x08, 0x89, 0xb7,
	0x70, 0xa3, 0x88, 0x3e, 0x4b, 0x62, 0x5f, 0x18, 0x8a, 0xd7, 0xbd, 0xd1, 0xf6, 0x1b, 0x8f, 0x34,
	0x9d, 0x28, 0x77, 0xb2, 0x61, 0xa8, 0x13, 0xab, 0x1d, 0x84, 0x4a, 0x2e, 0xc6, 0x18, 0x6a, 0x8f,
	0xb1, 0xbf, 0x92, 0xf6, 0xf9, 0x78, 0x7e, 0xce, 0xcc, 0x7b, 0x71, 0x94, 0xd2, 0x28, 0xcd, 0x87,
	0xa2, 0x7c, 0x0b, 0x05, 0xc6, 0x27, 0xf6, 0xa0, 0xb5, 0x74, 0x19, 0x8d, 0xd2, 0xc0, 0x97, 0x0e,
	0xf2, 0xed, 0xbf, 0xd6, 0x83, 0x19, 0x1c, 0x95, 0x72, 0x43, 0x8c, 0x38, 0x0d, 0xbe, 0xad, 0xf0,
	0x07, 0x68, 0x04, 0x61, 0x66, 0x86, 0x47, 0xa7, 0x73, 0x7d, 0xf1, 0xaf, 0xc7, 0xad, 0x7b, 0x56,
	0x21, 0x1f, 0xfc, 0x16, 0xe0, 0xff, 0x72, 0x4f, 0xd9, 0xfb, 0x5e, 0x8a, 0x81, 0x50, 0x8a, 0xc1,
	0x3a, 0x3b, 0x95, 0xcd, 0xec, 0xbc, 0x87, 0x16, 0x65, 0x8c, 0x32, 0x12, 0xc5, 0x3c, 0x87, 0xe2,
	0xf5, 0xe9, 0x86, 0x8f, 0x47, 0xfe, 0x05, 0x54, 0xcb, 0x32, 0x4c, 0xa2, 0x98, 0x63, 0xd5, 0x6a,
	0x72, 0xb1, 0x11, 0x97, 0xc2, 0x5f, 0x2b, 0x87, 0x7f, 0x08, 0x28, 0x48, 0x48, 0xf1, 0x62, 0x71,
	0xf4, 0x1c, 0x44, 0x94, 0x67, 0xb6, 0x65, 0x89, 0x41, 0xe2, 0x70, 0xd8, 0xe4, 0xe8, 0xd5, 0x3d,
	0xc0, 0xfa, 0x31, 0xf0, 0x11, 0xe0, 0x75, 0x45, 0xc6, 0xea, 0xad, 0x3c, 0x9f, 0x38, 0xe8, 0x3f,
	0x8c, 0x41, 0xdc, 0xc0, 0x6d, 0xdd, 0x46, 0x02, 0x3e, 0x80, 0xee, 0x06, 0x36, 0x9b, 0xa1, 0xca,
	0xd5, 0xaf, 0x6a, 0xf6, 0x4d, 0x89, 0x62, 0x1a, 0x8e, 0x6a, 0x38, 0xc4, 0x79, 0x98, 0xaa, 0x58,
	0x82, 0x37, 0x25, 0x88, 0x38, 0xea, 0x97, 0xac, 0xe9, 0x25, 0x9c, 0x97, 0x19, 0xfb, 0xce, 0xb4,
	0x1c, 0xa2, 0xab, 0xb6, 0x2d, 0x7f, 0x52, 0x91, 0x80, 0xcf, 0x40, 0x2a, 0x4b, 0xb2, 0x42, 0x56,
	0x1c, 0x1b, 0x55, 0x76, 0x35, 0xb0, 0x54, 0xc5, 0xd4, 0x75, 0xd5, 0x18, 0x13, 0x79, 0x3a, 0x45,
	0x55, 0xfc, 0x16, 0x7a, 0x65, 0x89, 0x61, 0x92, 0xb1, 0x66, 0x3b, 0x73, 0xeb, 0x06, 0xd5, 0x76,
	0xb5, 0x98, 0x98, 0x8a, 0xec, 0x68, 0xa6, 0x61, 0xdf, 0xc9, 0x96, 0x8a, 0xea, 0xf8, 0x04, 0x0e,
	0xcb, 0x12, 0x4d, 0xcf, 0xec, 0x35, 0x5e, 0xd7, 0xb5, 0x49, 0xc9, 0x0e, 0x6a, 0xe2, 0xe3, 0x2c,
	0x21, 0xdb, 0xf8, 0xdc, 0x9a, 0xa0, 0xd6, 0xae, 0x5e, 0xf2, 0x7c, 0xac, 0x99, 0xa8, 0xbd, 0x8b,
	0xfa, 0xac, 0x8d, 0x55, 0x13, 0xc1, 0xae, 0x2d, 0xbc, 0x9a, 0x44, 0x9d, 0x5d, 0x0b, 0xbe, 0xd5,
	0x26, 0x2a, 0xda, 0xc3, 0xa7, 0x70, 0x5c, 0x66, 0x6e, 0xb4, 0xaf, 0x8a, 0x6c, 0x8d, 0x51, 0x17,
	0xf7, 0xb2, 0x9f, 0xb1, 0x4d, 0xde, 0xcb, 0xb3, 0xb9, 0x6c, 0x20, 0xf1, 0x4f, 0x00, 0x00, 0x00,
	0xff, 0xff, 0x45, 0x24, 0xa4, 0xd2, 0xa6, 0x05, 0x00, 0x00,
}